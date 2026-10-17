#include "VuMeter.h"
#include "SuomiLookAndFeel.h"

VuMeterComponent::VuMeterComponent (const juce::String& labelText)
    : title (labelText)
{
    startTimer (kTimerMs);
}

VuMeterComponent::~VuMeterComponent()
{
    stopTimer();
}

void VuMeterComponent::setLevelSources (std::atomic<float>* leftLinear,
                                        std::atomic<float>* rightLinear)
{
    sourceL = leftLinear;
    sourceR = rightLinear;
}

void VuMeterComponent::setClipSources (std::atomic<float>* preClipL,
                                       std::atomic<float>* preClipR,
                                       std::atomic<float>* threshold,
                                       std::atomic<int>*   modeState)
{
    clipPrePeakSrcL  = preClipL;
    clipPrePeakSrcR  = preClipR;
    clipThresholdSrc = threshold;
    clipModeSrc      = modeState;
}

//==============================================================================
void VuMeterComponent::resized()
{
    auto b = getLocalBounds().reduced (3);

    labelArea = b.removeFromTop (14);
    b.removeFromTop (3);  // gap below label

    const int barW = (b.getWidth() - 4) / 2;
    barAreaL = b.removeFromLeft (barW);
    b.removeFromLeft (4);  // gap between the two bars
    barAreaR = b;
}

//==============================================================================
void VuMeterComponent::timerCallback()
{
    if (sourceL == nullptr || sourceR == nullptr)
        return;

    const float rawL = sourceL->load (std::memory_order_relaxed);
    const float rawR = sourceR->load (std::memory_order_relaxed);

    // Instant rise, exponential fall.
    displayL = juce::jmax (displayL * kDecayCoeff, rawL);
    displayR = juce::jmax (displayR * kDecayCoeff, rawR);

    // Peak hold: latch at new maximum, hold for N ticks, then decay.
    auto updatePeak = [&] (float raw, float& peak, int& ticks)
    {
        if (raw >= peak)  { peak = raw;  ticks = kPeakHoldTicks; }
        else if (ticks > 0) { --ticks; }
        else peak *= kDecayCoeff;
    };

    updatePeak (rawL, peakL, peakTicksL);
    updatePeak (rawR, peakR, peakTicksR);

    // Clip overlay — only updated when all clip sources are connected.
    if (clipModeSrc != nullptr && clipThresholdSrc != nullptr
        && clipPrePeakSrcL != nullptr && clipPrePeakSrcR != nullptr)
    {
        displayClipMode = clipModeSrc->load (std::memory_order_relaxed);

        if (displayClipMode >= 0)
        {
            const float preL = clipPrePeakSrcL->load (std::memory_order_relaxed);
            const float preR = clipPrePeakSrcR->load (std::memory_order_relaxed);
            displayClipPreL  = juce::jmax (displayClipPreL  * kDecayCoeff, preL);
            displayClipPreR  = juce::jmax (displayClipPreR  * kDecayCoeff, preR);
            displayClipThresh = clipThresholdSrc->load (std::memory_order_relaxed);
        }
        else
        {
            // Clip disabled — clear the overlay state immediately.
            displayClipPreL  = 0.0f;
            displayClipPreR  = 0.0f;
            displayClipThresh = 1.0f;
        }
    }

    repaint();
}

//==============================================================================
float VuMeterComponent::linearToFill (float linear)
{
    if (linear <= 0.0f)
        return 0.0f;
    const float dB = 20.0f * std::log10 (linear);
    return juce::jlimit (0.0f, 1.0f, (dB - kDbFloor) / (-kDbFloor));
}

juce::Colour VuMeterComponent::segmentColour (float fillPos)
{
    if (fillPos < 0.80f) return juce::Colour (0xff22cc44);  // green
    if (fillPos < 0.90f) return juce::Colour (0xffffcc00);  // yellow
    return juce::Colour (0xffff2222);                        // red
}

void VuMeterComponent::drawBar (juce::Graphics& g,
                                juce::Rectangle<int> area,
                                float displayLevel,
                                float peakLevel) const
{
    // Dark well behind the fill.
    g.setColour (juce::Colour (0xff10101e));
    g.fillRect (area);

    const float fillFrac = linearToFill (displayLevel);
    const float peakFrac = linearToFill (peakLevel);
    const int   totalH   = area.getHeight();

    // Fill the bar from the bottom up, using colour zones.
    // Zone boundaries in fill-fraction: 0..0.80 green, 0.80..0.90 yellow, 0.90..1.0 red.
    struct Zone { float lo, hi; juce::Colour colour; };
    const Zone zones[] = {
        { 0.00f, 0.80f, juce::Colour (0xff22cc44) },
        { 0.80f, 0.90f, juce::Colour (0xffffcc00) },
        { 0.90f, 1.00f, juce::Colour (0xffff2222) }
    };

    for (const auto& zone : zones)
    {
        // In screen coords: silence is at the bottom (high Y), signal at the top (low Y).
        // Zone occupies screen rows (1-zone.hi)*totalH .. (1-zone.lo)*totalH from top.
        const int zoneTop = area.getY() + juce::roundToInt ((1.0f - zone.hi) * totalH);
        const int zoneBot = area.getY() + juce::roundToInt ((1.0f - zone.lo) * totalH);
        const int fillTop = area.getBottom() - juce::roundToInt (fillFrac * totalH);

        const int drawTop = juce::jmax (fillTop, zoneTop);
        const int drawBot = juce::jmin (area.getBottom(), zoneBot);

        if (drawTop < drawBot)
        {
            g.setColour (zone.colour);
            g.fillRect (area.getX(), drawTop, area.getWidth(), drawBot - drawTop);
        }
    }

    // Thin dark grid lines every 6 dB to give a segmented look.
    g.setColour (juce::Colour (0x33000000));
    static constexpr float kDbStep = 6.0f;
    for (float dB = kDbFloor + kDbStep; dB <= 0.0f; dB += kDbStep)
    {
        const float frac = (dB - kDbFloor) / (-kDbFloor);
        const int   lineY = area.getY() + juce::roundToInt ((1.0f - frac) * totalH);
        g.fillRect (area.getX(), lineY, area.getWidth(), 1);
    }

    // Peak hold indicator — a bright two-pixel line.
    if (peakLevel > 0.0f)
    {
        const int peakY = area.getY() + juce::roundToInt ((1.0f - peakFrac) * totalH);
        if (peakY >= area.getY() && peakY < area.getBottom())
        {
            g.setColour (segmentColour (peakFrac).brighter (0.4f));
            g.fillRect (area.getX(), peakY, area.getWidth(), 2);
        }
    }
}

//==============================================================================
void VuMeterComponent::drawClipOverlay (juce::Graphics& g,
                                        juce::Rectangle<int> area,
                                        float preClipPeak,
                                        float threshold,
                                        int /*mode*/) const
{
    const float threshFill  = linearToFill (threshold);
    const float preFill     = linearToFill (preClipPeak);
    const int   totalH      = area.getHeight();

    // threshY: screen Y of the clip ceiling (lower Y = higher on screen = louder).
    const int threshY     = area.getY() + juce::roundToInt ((1.0f - threshFill) * totalH);
    const bool isClipping = preClipPeak > threshold * 1.001f;

    // Ensure the overlay zone is always visible (at least 6 px tall).
    const int minZoneH    = 6;
    const int zoneTop     = juce::jmin (threshY - minZoneH, area.getY());
    const int zoneBottom  = threshY;
    const int zoneH       = zoneBottom - zoneTop;

    if (zoneH <= 0)
        return;

    // Gradient: opaque orange at the top, transparent at the threshold.
    const juce::uint32 topColour = isClipping ? 0x88ff8800u : 0x3dff8800u;
    juce::ColourGradient grad (juce::Colour (topColour),
                               0.0f, static_cast<float> (zoneTop),
                               juce::Colour (0x00ff8800),
                               0.0f, static_cast<float> (zoneBottom),
                               false);
    g.setGradientFill (grad);
    g.fillRect (area.getX(), zoneTop, area.getWidth(), zoneH);

    // Solid threshold line (2 px) — always clearly visible when clip is active.
    g.setColour (isClipping ? juce::Colour (0xddff9900)
                            : juce::Colour (0x88ff9900));
    g.fillRect (area.getX(), threshY - 1, area.getWidth(), 2);

    // Pre-clip peak marker — shows how far above the ceiling the raw signal reached.
    if (isClipping && preFill > threshFill)
    {
        const int prePeakY = area.getY() + juce::roundToInt ((1.0f - preFill) * totalH);
        g.setColour (juce::Colour (0xaaff5500));
        g.fillRect (area.getX(), prePeakY, area.getWidth(), 2);
    }
}

//==============================================================================
void VuMeterComponent::paint (juce::Graphics& g)
{
    g.setColour (SuomiLookAndFeel::kPurpleDark);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

    g.setColour (SuomiLookAndFeel::kPurple);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont   (juce::Font (10.0f, juce::Font::bold));
    g.drawText  (title, labelArea, juce::Justification::centred);

    drawBar (g, barAreaL, displayL, peakL);
    drawBar (g, barAreaR, displayR, peakR);

    if (displayClipMode >= 0)
    {
        drawClipOverlay (g, barAreaL, displayClipPreL, displayClipThresh, displayClipMode);
        drawClipOverlay (g, barAreaR, displayClipPreR, displayClipThresh, displayClipMode);
    }
}
