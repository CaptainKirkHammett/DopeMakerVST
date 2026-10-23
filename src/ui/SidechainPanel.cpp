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
    addAndMakeVisible (modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::sidechainMode, modeBox);

    // Display text and dimming are handled together in applyDim (below).

    // ── Division ──────────────────────────────────────────────────────────────
    divLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (divLabel);

    divBox.addItem ("1/64", 12);
    const juce::StringArray divNames {
        "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
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
        // Use the green pointer style shared with non-FX knobs.
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

    // ── Dim controls when Off ─────────────────────────────────────────────────
    // Called on every mode change and once at construction to set the initial state.
    // getSelectedId() returns 0 when nothing is selected yet (e.g. during
    // construction before the attachment pushes the initial value), so treat
    // both 0 and 1 as "off".
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
        // Mirror ToggleButton on/off colours so the combo looks like a toggle.
        modeBox.setColour (juce::ComboBox::backgroundColourId,
                           isOff ? SuomiLookAndFeel::kPurpleDark.withAlpha (0.75f)
                                 : SuomiLookAndFeel::kPurple   .withAlpha (0.85f));
        modeBox.setColour (juce::ComboBox::outlineColourId,
                           SuomiLookAndFeel::kPurple.withAlpha (0.55f));
        modeBox.setColour (juce::ComboBox::textColourId,
                           isOff ? juce::Colours::white.withAlpha (0.55f)
                                 : juce::Colours::white);
        // Show "Sidechain" on the button when off, but the full mode name when active.
        if (isOff)
            modeBox.setText ("Sidechain", juce::dontSendNotification);
        repaint();
    };
    modeBox.onChange = applyDim;
    applyDim();   // initialise from the restored parameter value

    modeBox.setJustificationType (juce::Justification::centred);
    // Render without the drop-down arrow, matching the ToggleButton visual style.
    modeBox.getProperties().set ("toggleStyle", true);
}

//==============================================================================
void SidechainPanel::resized()
{
    auto b = getLocalBounds().reduced (5, 0);
    b.setX (2);   // left padding 2px instead of 5 — closes gap to step controller

    constexpr int comboH = 24;
    constexpr int gap    = 3;
    constexpr int lblH   = 12;

    // Reserve the right column by width upfront.
    const int vizW   = b.getWidth() * 2 / 5;
    auto rightCol    = b.removeFromRight (vizW);
    b.removeFromRight (4);   // gap between controls and viz

    // ── Left column: mode combo only, then straight to knobs ─────────────────
    modeBox.setBounds (b.removeFromTop (comboH));
    b.removeFromTop (gap);
    // Division row removed from here — it now lives above the viz (see below).

    // ── Five knobs side-by-side in remaining left column ─────────────────────
    const int colW   = (b.getWidth() - 4 * gap) / 5;
    const int knobSz = juce::jmin (colW, b.getHeight() - lblH);

    // ── Right column: divBox on top (same width as viz), viz below ────────────
    // Label no longer shown — the combo is self-explanatory at this size.
    divLabel.setVisible (false);
    divBox.setBounds (rightCol.removeFromTop (comboH));
    rightCol.removeFromTop (gap);

    // Viz: top is now below the divBox; bottom is tight to the knob content.
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
}

//==============================================================================
void SidechainPanel::drawEnvelopeViz (juce::Graphics& g) const
{
    const float preDelay = (float) preDelaySlider.getValue();
    const float rise     = juce::jlimit (0.01f, 1.0f, (float) riseSlider.getValue());
    const float curve    = (float) curveSlider.getValue();
    const float fall     = (float) fallSlider.getValue();
    const float mix      = (float) mixSlider.getValue();

    const float vx = (float) vizArea.getX();
    const float vy = (float) vizArea.getY();
    const float vw = (float) vizArea.getWidth();
    const float vh = (float) vizArea.getHeight();
    const float vb = vy + vh;   // bottom Y

    const bool isOff = (modeBox.getSelectedId() == 1);

    // ── Background — noticeably lighter when Off so the state is visible ───────
    g.setColour (isOff ? juce::Colour (0xff2a2a2a) : juce::Colour (0xff10101e));
    g.fillRect  (vizArea);

    g.saveState();
    g.reduceClipRegion (vizArea);

    // ── Reference lines ───────────────────────────────────────────────────────
    // Top = full volume, bottom = silence.
    // Dim top line (gain = 1.0).
    g.setColour (juce::Colour (0x33ffffff));
    g.drawHorizontalLine (juce::roundToInt (vy + 1), vx, vx + vw);

    // ── Build the volume envelope path ────────────────────────────────────────
    // gain [0..1]: 0 = bottom (silent), 1 = top (full volume).
    // gainToY: maps 0 → vb (bottom), 1 → vy (top).
    auto gainToY = [&] (float gainVal) -> float
    {
        return vb - juce::jlimit (0.0f, vh, gainVal * vh);
    };

    // Pre-compute constants shared across all path points — mirrors computeEnvelope().
    const float exponent    = std::pow (2.0f, 3.0f - 6.0f * curve);
    const float totalSpan   = juce::jmax (1e-5f, 1.0f - preDelay);
    const float riseSpan    = juce::jmax (1e-5f, rise * totalSpan);
    const float riseEnd     = preDelay + riseSpan;
    const float maxFallSpan = juce::jmax (0.0f, 1.0f - riseEnd);
    const float fallSpan    = (1.0f - fall) * maxFallSpan;
    const float fallStart   = 1.0f - fallSpan;

    juce::Path curvePath;
    const int steps = juce::jmax (2, (int) vw);

    for (int px = 0; px < steps; ++px)
    {
        const float t = (float) px / (float) (steps - 1);   // 0..1 through the period

        // Mirror the DSP envelope exactly: muted → shaped rise → hold → shaped fall.
        float env;
        if (t <= preDelay)
        {
            env = 0.0f;
        }
        else if (t < riseEnd)
        {
            const float pos = juce::jlimit (0.0f, 1.0f, (t - preDelay) / riseSpan);
            env = std::pow (pos, exponent);
        }
        else if (fallSpan > 1e-5f && t >= fallStart)
        {
            const float pos = juce::jlimit (0.0f, 1.0f, (t - fallStart) / fallSpan);
            env = 1.0f - pos;   // linear — fall shape is independent of the curve knob
        }
        else
        {
            env = 1.0f;   // hold region: volume fully open
        }

        // Applied gain with mix: at mix=1 starts from true silence (gain=0).
        const float displayGain = juce::jmax (0.0f, 1.0f + mix * (env - 1.0f));
        const float y = gainToY (displayGain);
        const float x = vx + t * vw;

        if (px == 0)
            curvePath.startNewSubPath (x, y);
        else
            curvePath.lineTo (x, y);
    }

    const juce::Colour lineColour = isOff ? juce::Colour (0xff555555)
                                          : SuomiLookAndFeel::kKnobPointer;

    // ── Filled area below the curve (= signal that passes through) ───────────
    juce::Path fillPath (curvePath);
    fillPath.lineTo (vx + vw, vb);
    fillPath.lineTo (vx,      vb);
    fillPath.closeSubPath();
    g.setColour (lineColour.withAlpha (0.18f));
    g.fillPath  (fillPath);

    // ── Curve line ────────────────────────────────────────────────────────────
    g.setColour (lineColour);
    g.strokePath (curvePath, juce::PathStrokeType (1.5f));

    g.restoreState();

    // ── Border ────────────────────────────────────────────────────────────────
    g.setColour (SuomiLookAndFeel::kPurple);
    g.drawRect  (vizArea, 1);
}

//==============================================================================
void SidechainPanel::paint (juce::Graphics& g)
{
    // Same background as the GateControllerPanel.
    // Round top corners only — bottom edge is shared with ArpeggiatorPanel.
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
