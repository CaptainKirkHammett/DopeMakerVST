#include "SidechainPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"
#include <cmath>

SidechainPanel::SidechainPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts)
{
    // ── Mode selector ─────────────────────────────────────────────────────────
    modeBox.addItem ("Sidechain Off",  1);
    modeBox.addItem ("Sidechain Pre",  2);
    modeBox.addItem ("Sidechain Post", 3);
    modeBox.addItem ("Sidechain FX Only", 4);
    addAndMakeVisible (modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::sidechainMode, modeBox);

    // ── Division ──────────────────────────────────────────────────────────────
    divLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (divLabel);

    const juce::StringArray divNames {
        "1/64","1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
    };
    for (int i = 0; i < divNames.size(); ++i)
        divBox.addItem (divNames[i], i + 1);
    addAndMakeVisible (divBox);
    divAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::sidechainDivision, divBox);

    // ── Knobs (rotary, no text box) ───────────────────────────────────────────
    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const char* id)
    {
        sl.setSliderStyle    (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle   (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        sl.onValueChange = [this] { repaint(); };
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    mixAttach      = makeKnob (mixSlider,      mixLabel,      ParamIDs::sidechainMix);
    preDelayAttach = makeKnob (preDelaySlider, preDelayLabel, ParamIDs::sidechainPreDelay);
    riseAttach     = makeKnob (riseSlider,     riseLabel,     ParamIDs::sidechainRise);
    curveAttach    = makeKnob (curveSlider,    curveLabel,    ParamIDs::sidechainCurve);
    fallAttach     = makeKnob (fallSlider,     fallLabel,     ParamIDs::sidechainFall);

    // ── "Knob Mode" button — switches back from draw mode ────────────────────
    knobModeButton.onClick = [this]
    {
        if (auto* param = apvtsRef.getParameter (ParamIDs::sidechainDrawMode))
            param->setValueNotifyingHost (0.0f);
        applyDrawModeUI();
    };
    addChildComponent (knobModeButton);   // hidden until draw mode is on

    // ── Dim controls when Off ─────────────────────────────────────────────────
    auto applyDim = [this]
    {
        const int   selId = modeBox.getSelectedId();
        const bool  isOff = (selId <= 1);
        const float a     = isOff ? 0.35f : 1.0f;
        mixSlider     .setAlpha (a);  preDelaySlider.setAlpha (a);
        riseSlider    .setAlpha (a);  curveSlider   .setAlpha (a);
        fallSlider    .setAlpha (a);  fallLabel     .setAlpha (a);
        mixLabel      .setAlpha (a);  preDelayLabel .setAlpha (a);
        riseLabel     .setAlpha (a);  curveLabel    .setAlpha (a);
        divLabel      .setAlpha (a);  divBox        .setAlpha (a);
        modeBox.setColour (juce::ComboBox::backgroundColourId,
                           isOff ? SuomiLookAndFeel::kPurpleDark.withAlpha (0.75f)
                                 : SuomiLookAndFeel::kPurple   .withAlpha (0.85f));
        modeBox.setColour (juce::ComboBox::outlineColourId,
                           SuomiLookAndFeel::kPurple.withAlpha (0.55f));
        modeBox.setColour (juce::ComboBox::textColourId,
                           isOff ? juce::Colours::white.withAlpha (0.55f)
                                 : juce::Colours::white);
        if (isOff)
            modeBox.setText ("Sidechain", juce::dontSendNotification);
        applyDrawModeUI();
        repaint();
    };
    modeBox.onChange = applyDim;
    applyDim();

    modeBox.setJustificationType (juce::Justification::centred);
    modeBox.getProperties().set ("toggleStyle", true);
}

//==============================================================================
void SidechainPanel::setDrawnEnvelopeTable (float* table, int tableSize)
{
    drawnTable     = table;
    drawnTableSize = tableSize;
}

bool SidechainPanel::isDrawMode() const
{
    auto* p = apvtsRef.getRawParameterValue (ParamIDs::sidechainDrawMode);
    return p != nullptr && p->load() > 0.5f;
}

void SidechainPanel::applyDrawModeUI()
{
    const bool dm   = isDrawMode();
    const bool isOff = (modeBox.getSelectedId() <= 1);

    // In draw mode: dim the four shape knobs and show the "Knob Mode" button.
    const float shapeAlpha = (isOff) ? 0.35f : (dm ? 0.25f : 1.0f);
    preDelaySlider.setAlpha (shapeAlpha);  preDelayLabel.setAlpha (shapeAlpha);
    riseSlider    .setAlpha (shapeAlpha);  riseLabel    .setAlpha (shapeAlpha);
    curveSlider   .setAlpha (shapeAlpha);  curveLabel   .setAlpha (shapeAlpha);
    fallSlider    .setAlpha (shapeAlpha);  fallLabel    .setAlpha (shapeAlpha);

    // Disable interaction on shape knobs when in draw mode.
    preDelaySlider.setEnabled (!dm && !isOff);
    riseSlider    .setEnabled (!dm && !isOff);
    curveSlider   .setEnabled (!dm && !isOff);
    fallSlider    .setEnabled (!dm && !isOff);

    knobModeButton.setVisible (dm && !isOff);
    repaint();
}

//==============================================================================
void SidechainPanel::copyKnobEnvelopeToTable()
{
    if (drawnTable == nullptr || drawnTableSize <= 0)
        return;

    const float preDelay = (float) preDelaySlider.getValue();
    const float rise     = juce::jlimit (0.01f, 1.0f, (float) riseSlider.getValue());
    const float curve    = (float) curveSlider.getValue();
    const float fall     = (float) fallSlider.getValue();

    const float exponent    = std::pow (2.0f, 3.0f - 6.0f * curve);
    const float totalSpan   = juce::jmax (1e-5f, 1.0f - preDelay);
    const float riseSpan    = juce::jmax (1e-5f, rise * totalSpan);
    const float riseEnd     = preDelay + riseSpan;
    const float maxFallSpan = juce::jmax (0.0f, 1.0f - riseEnd);
    const float fallSpan    = (1.0f - fall) * maxFallSpan;
    const float fallStart   = 1.0f - fallSpan;

    for (int i = 0; i < drawnTableSize; ++i)
    {
        const float t = (float) i / (float) (drawnTableSize - 1);
        float env;

        if (t <= preDelay)
            env = 0.0f;
        else if (t < riseEnd)
        {
            const float pos = juce::jlimit (0.0f, 1.0f, (t - preDelay) / riseSpan);
            env = std::pow (pos, exponent);
        }
        else if (fallSpan > 1e-5f && t >= fallStart)
        {
            const float pos = juce::jlimit (0.0f, 1.0f, (t - fallStart) / fallSpan);
            env = 1.0f - pos;
        }
        else
            env = 1.0f;

        drawnTable[i] = env;
    }
}

void SidechainPanel::paintToTable (int pixelX0, int pixelY0, int pixelX1, int pixelY1)
{
    if (drawnTable == nullptr || drawnTableSize <= 0 || vizArea.getWidth() <= 0)
        return;

    const float vx = (float) vizArea.getX();
    const float vw = (float) vizArea.getWidth();
    const float vy = (float) vizArea.getY();
    const float vh = (float) vizArea.getHeight();
    const float last = (float) (drawnTableSize - 1);

    // Convert pixel positions to table indices and gain values.
    const float t0 = juce::jlimit (0.0f, 1.0f, ((float) pixelX0 - vx) / vw);
    const float t1 = juce::jlimit (0.0f, 1.0f, ((float) pixelX1 - vx) / vw);
    const float g0 = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) pixelY0 - vy) / vh);
    const float g1 = juce::jlimit (0.0f, 1.0f, 1.0f - ((float) pixelY1 - vy) / vh);

    int idx0 = juce::jlimit (0, drawnTableSize - 1, juce::roundToInt (t0 * last));
    int idx1 = juce::jlimit (0, drawnTableSize - 1, juce::roundToInt (t1 * last));

    // Walk every table index between the two endpoints, interpolating gain.
    if (idx0 == idx1)
    {
        drawnTable[idx0] = g1;
        return;
    }

    const int step = (idx1 > idx0) ? 1 : -1;
    const int count = std::abs (idx1 - idx0);
    for (int i = 0; i <= count; ++i)
    {
        const float frac = (float) i / (float) count;
        const float gain = g0 + frac * (g1 - g0);
        drawnTable[idx0 + i * step] = gain;
    }
}

void SidechainPanel::smoothDrawnTable()
{
    if (drawnTable == nullptr || drawnTableSize < 3)
        return;

    // Simple 3-tap moving average, applied twice for a smoother result.
    for (int pass = 0; pass < 2; ++pass)
    {
        float prev = drawnTable[0];
        for (int i = 1; i < drawnTableSize - 1; ++i)
        {
            const float cur = drawnTable[i];
            drawnTable[i] = (prev + cur + drawnTable[i + 1]) / 3.0f;
            prev = cur;
        }
    }
}

//==============================================================================
// Mouse interaction for the viz area draw mode.
void SidechainPanel::mouseMove (const juce::MouseEvent& e)
{
    const bool wasHovering = hoveringViz;
    hoveringViz = vizArea.contains (e.getPosition()) && !isDrawMode()
                  && modeBox.getSelectedId() > 1;

    if (hoveringViz != wasHovering)
    {
        // Tooltip text drawn directly in the viz area paint method.
        repaint();
    }
}

void SidechainPanel::mouseExit (const juce::MouseEvent&)
{
    if (hoveringViz)
    {
        hoveringViz = false;
        // (hover hint drawn in paint)
        repaint();
    }
}

void SidechainPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! vizArea.contains (e.getPosition()))
        return;
    if (modeBox.getSelectedId() <= 1)
        return;

    // If not in draw mode yet, enter it without drawing — the first click
    // only activates draw mode.  Subsequent clicks/drags will draw.
    if (! isDrawMode())
    {
        if (! hasDrawnOnce)
            copyKnobEnvelopeToTable();

        if (auto* param = apvtsRef.getParameter (ParamIDs::sidechainDrawMode))
            param->setValueNotifyingHost (1.0f);
        applyDrawModeUI();
        hoveringViz = false;
        repaint();
        return;
    }

    isDrawing = true;
    hasDrawnOnce = true;
    lastDrawX = e.getPosition().getX();
    lastDrawY = juce::jlimit (vizArea.getY(), vizArea.getBottom(), e.getPosition().getY());
    paintToTable (lastDrawX, lastDrawY, lastDrawX, lastDrawY);
    repaint();
}

void SidechainPanel::mouseDrag (const juce::MouseEvent& e)
{
    if (! isDrawing)
        return;

    const int curX = e.getPosition().getX();
    const int curY = juce::jlimit (vizArea.getY(), vizArea.getBottom(), e.getPosition().getY());

    // Linearly interpolate between previous and current point.
    if (lastDrawX >= 0)
        paintToTable (lastDrawX, lastDrawY, curX, curY);
    else
        paintToTable (curX, curY, curX, curY);

    lastDrawX = curX;
    lastDrawY = curY;
    repaint();
}

void SidechainPanel::mouseUp (const juce::MouseEvent&)
{
    if (isDrawing)
    {
        smoothDrawnTable();
        repaint();
    }
    isDrawing = false;
    lastDrawX = -1;
    lastDrawY = -1;
}

//==============================================================================
void SidechainPanel::resized()
{
    auto b = getLocalBounds().reduced (5, 0);
    b.setX (2);

    constexpr int comboH = 24;
    constexpr int gap    = 3;
    constexpr int lblH   = 12;

    const int vizW   = b.getWidth() * 2 / 5;
    auto rightCol    = b.removeFromRight (vizW);
    b.removeFromRight (4);

    modeBox.setBounds (b.removeFromTop (comboH));
    b.removeFromTop (gap);

    const int colW   = (b.getWidth() - 4 * gap) / 5;
    const int knobSz = juce::jmin (colW, b.getHeight() - lblH);

    divLabel.setVisible (false);
    divBox.setBounds (rightCol.removeFromTop (comboH));
    rightCol.removeFromTop (gap);

    vizArea = rightCol.withBottom (b.getY() + lblH + knobSz);

    auto placeKnob = [&] (juce::Label& lb, juce::Slider& sl)
    {
        auto col = b.removeFromLeft (colW);
        lb.setBounds (col.removeFromTop (lblH));
        lb.setJustificationType (juce::Justification::centred);
        sl.setBounds (col.removeFromTop (knobSz).withSizeKeepingCentre (knobSz, knobSz));
        b.removeFromLeft (gap);
    };

    placeKnob (mixLabel,      mixSlider);
    placeKnob (preDelayLabel, preDelaySlider);
    placeKnob (riseLabel,     riseSlider);
    placeKnob (curveLabel,    curveSlider);
    placeKnob (fallLabel,     fallSlider);

    // "Knob Mode" button centered over the four shape knobs (Pre..Fall).
    {
        auto preArea  = preDelaySlider.getBounds();
        auto fallArea = fallSlider.getBounds();
        auto btnArea  = preArea.getUnion (fallArea);
        knobModeButton.setBounds (btnArea.withSizeKeepingCentre (
            juce::jmin (btnArea.getWidth(), 80), 18));
    }
}

//==============================================================================
void SidechainPanel::drawEnvelopeViz (juce::Graphics& g) const
{
    const float mix = (float) mixSlider.getValue();

    const float vx = (float) vizArea.getX();
    const float vy = (float) vizArea.getY();
    const float vw = (float) vizArea.getWidth();
    const float vh = (float) vizArea.getHeight();
    const float vb = vy + vh;

    const bool isOff = (modeBox.getSelectedId() <= 1);
    const bool dm    = isDrawMode();

    // ── Background ────────────────────────────────────────────────────────────
    g.setColour (isOff ? juce::Colour (0xff2a2a2a) : juce::Colour (0xff10101e));
    g.fillRect  (vizArea);

    g.saveState();
    g.reduceClipRegion (vizArea);

    // Reference line at top (gain = 1.0).
    g.setColour (juce::Colour (0x33ffffff));
    g.drawHorizontalLine (juce::roundToInt (vy + 1), vx, vx + vw);

    auto gainToY = [&] (float gainVal) -> float
    {
        return vb - juce::jlimit (0.0f, vh, gainVal * vh);
    };

    juce::Path curvePath;
    const int steps = juce::jmax (2, (int) vw);

    if (dm && drawnTable != nullptr && drawnTableSize > 0)
    {
        // ── Draw mode: render from the table ─────────────────────────────────
        for (int px = 0; px < steps; ++px)
        {
            const float t   = (float) px / (float) (steps - 1);
            const float idx = t * (float) (drawnTableSize - 1);
            const int   i0  = juce::jlimit (0, drawnTableSize - 1, (int) idx);
            const int   i1  = juce::jlimit (0, drawnTableSize - 1, i0 + 1);
            const float frac = idx - (float) i0;
            const float env = drawnTable[i0] + frac * (drawnTable[i1] - drawnTable[i0]);

            const float displayGain = juce::jmax (0.0f, 1.0f + mix * (env - 1.0f));
            const float y = gainToY (displayGain);
            const float x = vx + t * vw;

            if (px == 0) curvePath.startNewSubPath (x, y);
            else         curvePath.lineTo (x, y);
        }
    }
    else
    {
        // ── Knob mode: compute from knob values ─────────────────────────────
        const float preDelay = (float) preDelaySlider.getValue();
        const float rise     = juce::jlimit (0.01f, 1.0f, (float) riseSlider.getValue());
        const float curve    = (float) curveSlider.getValue();
        const float fall     = (float) fallSlider.getValue();

        const float exponent    = std::pow (2.0f, 3.0f - 6.0f * curve);
        const float totalSpan   = juce::jmax (1e-5f, 1.0f - preDelay);
        const float riseSpan    = juce::jmax (1e-5f, rise * totalSpan);
        const float riseEnd     = preDelay + riseSpan;
        const float maxFallSpan = juce::jmax (0.0f, 1.0f - riseEnd);
        const float fallSpan    = (1.0f - fall) * maxFallSpan;
        const float fallStart   = 1.0f - fallSpan;

        for (int px = 0; px < steps; ++px)
        {
            const float t = (float) px / (float) (steps - 1);
            float env;

            if (t <= preDelay)
                env = 0.0f;
            else if (t < riseEnd)
            {
                const float pos = juce::jlimit (0.0f, 1.0f, (t - preDelay) / riseSpan);
                env = std::pow (pos, exponent);
            }
            else if (fallSpan > 1e-5f && t >= fallStart)
            {
                const float pos = juce::jlimit (0.0f, 1.0f, (t - fallStart) / fallSpan);
                env = 1.0f - pos;
            }
            else
                env = 1.0f;

            const float displayGain = juce::jmax (0.0f, 1.0f + mix * (env - 1.0f));
            const float y = gainToY (displayGain);
            const float x = vx + t * vw;

            if (px == 0) curvePath.startNewSubPath (x, y);
            else         curvePath.lineTo (x, y);
        }
    }

    const juce::Colour lineColour = isOff ? juce::Colour (0xff555555)
                                          : SuomiLookAndFeel::kKnobPointer;

    // ── Filled area below the curve ──────────────────────────────────────────
    juce::Path fillPath (curvePath);
    fillPath.lineTo (vx + vw, vb);
    fillPath.lineTo (vx,      vb);
    fillPath.closeSubPath();
    g.setColour (lineColour.withAlpha (0.18f));
    g.fillPath  (fillPath);

    // ── Curve line ───────────────────────────────────────────────────────────
    g.setColour (lineColour);
    g.strokePath (curvePath, juce::PathStrokeType (1.5f));

    g.restoreState();

    // ── Hover hint ───────────────────────────────────────────────────────────
    if (hoveringViz)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (vizArea);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (11.0f);
        g.drawText ("Click to draw envelope", vizArea, juce::Justification::centred);
    }

    // ── Border — highlight when in draw mode ─────────────────────────────────
    g.setColour (dm && !isOff ? SuomiLookAndFeel::kKnobPointer.withAlpha (0.6f)
                              : SuomiLookAndFeel::kPurple);
    g.drawRect  (vizArea, 1);
}

//==============================================================================
void SidechainPanel::paint (juce::Graphics& g)
{
    g.setColour (SuomiLookAndFeel::kBackground.withAlpha (0.30f));
    {
        juce::Path p;
        auto b = getLocalBounds().toFloat();
        p.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(),
                               6.0f, 6.0f, true, true, false, false);
        g.fillPath (p);
    }

    drawEnvelopeViz (g);
}
