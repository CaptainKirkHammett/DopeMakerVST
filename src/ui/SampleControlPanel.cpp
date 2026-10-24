#include "SampleControlPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

SampleControlPanel::SampleControlPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvts (apvts)
{
    // Root note — C0..B8 (MIDI 0–119).
    for (int i = 0; i <= 119; ++i)
        rootNoteBox.addItem (juce::MidiMessage::getMidiNoteName (i, true, true, 4), i + 1);
    addAndMakeVisible (rootNoteBox);
    rootNoteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::rootNote, rootNoteBox);

    // Sample direction.
    sampleDirBox.addItem ("Sample FW", 1);
    sampleDirBox.addItem ("Sample BW", 2);
    sampleDirBox.addItem ("FW/BW Rnd", 3);
    addAndMakeVisible (sampleDirBox);
    sampleDirAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::sampleDirection, sampleDirBox);

    // Loop mode.
    loopModeBox.addItem ("No Loop",   1);
    loopModeBox.addItem ("Forward",   2);
    loopModeBox.addItem ("Ping-Pong", 3);
    addAndMakeVisible (loopModeBox);
    loopModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::loopMode, loopModeBox);

    // Semitone offset — rotary, value overlaid on knob face.
    semitoneSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    semitoneSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    semitoneSlider.getProperties().set ("semitoneKnob", true);
    semitoneSlider.setRange (-48.0, 48.0, 1.0);
    semitoneSlider.textFromValueFunction = [] (double v) {
        const int s = juce::roundToInt (v);
        return s > 0 ? ("+" + juce::String (s)) : juce::String (s);
    };
    semitoneSlider.valueFromTextFunction = [] (const juce::String& t) {
        return static_cast<double> (t.retainCharacters ("0123456789+-").getIntValue());
    };
    addAndMakeVisible (semitoneLabel);
    addAndMakeVisible (semitoneSlider);

    semitoneModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (semitoneModIndicator);

    // Overlay label — transparent background, sits on top of the knob.
    // "customFont" property opts this label into the getLabelFont override path
    // so setFont() is honoured instead of being replaced by the default caption size.
    semitoneValueLabel.getProperties().set ("customFont", true);
    semitoneValueLabel.setFont (juce::Font (26.0f, juce::Font::bold));
    semitoneValueLabel.setInterceptsMouseClicks (false, false);
    semitoneValueLabel.setJustificationType (juce::Justification::centred);
    semitoneValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (semitoneValueLabel);

    // Update overlay whenever the slider moves.
    semitoneSlider.onValueChange = [this] {
        const int s = juce::roundToInt (semitoneSlider.getValue());
        semitoneValueLabel.setText (s > 0 ? ("+" + juce::String (s)) : juce::String (s),
                                    juce::dontSendNotification);
    };
    // Initialise immediately so the label shows the restored value.
    semitoneSlider.onValueChange();

    semitoneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::semitoneOffset, semitoneSlider);

    // Gain (dB) — same style as the semitone knob.
    gainSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    gainSlider.getProperties().set ("semitoneKnob", true);
    gainSlider.setRange (-40.0, 12.0, 0.1);
    gainSlider.textFromValueFunction = [] (double v) {
        const juce::String s = juce::String (v, 1);
        return v > 0.0 ? ("+" + s + " dB") : (s + " dB");
    };
    gainSlider.valueFromTextFunction = [] (const juce::String& t) {
        return t.retainCharacters ("0123456789.+-").getDoubleValue();
    };
    addAndMakeVisible (gainLabel);
    addAndMakeVisible (gainSlider);

    gainValueLabel.getProperties().set ("customFont", true);
    gainValueLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    gainValueLabel.setInterceptsMouseClicks (false, false);
    gainValueLabel.setJustificationType (juce::Justification::centred);
    gainValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (gainValueLabel);

    gainSlider.onValueChange = [this] {
        const double v = gainSlider.getValue();
        const juce::String s = juce::String (std::abs (v), 1);
        gainValueLabel.setText (v >= 0.0 ? ("+" + s) : ("-" + s),
                                juce::dontSendNotification);
    };
    gainSlider.onValueChange();

    gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::gainDb, gainSlider);

    // Stereo width — same style as the other knobs.
    stereoSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    stereoSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    stereoSlider.getProperties().set ("semitoneKnob", true);
    stereoSlider.setRange (-1.0, 1.0, 0.01);
    stereoSlider.textFromValueFunction = [] (double v) {
        const int pct = juce::roundToInt (v * 100.0);
        return pct > 0 ? ("+" + juce::String (pct) + "%")
                       : (juce::String (pct) + "%");
    };
    stereoSlider.valueFromTextFunction = [] (const juce::String& t) {
        return t.retainCharacters ("0123456789.+-").getDoubleValue() / 100.0;
    };
    addAndMakeVisible (stereoLabel);
    addAndMakeVisible (stereoSlider);
    addAndMakeVisible (stereoGoniometer);

    stereoAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::stereoWidth, stereoSlider);

    // Limiter — same style as the gain knob.
    limiterSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    limiterSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    limiterSlider.getProperties().set ("semitoneKnob", true);
    limiterSlider.setRange (0.0, 1.0, 0.01);
    limiterSlider.textFromValueFunction = [] (double v) {
        if (v <= 0.0) return juce::String ("Off");
        return juce::String (juce::roundToInt (v * 100)) + "%";
    };
    limiterSlider.valueFromTextFunction = [] (const juce::String& t) {
        if (t.equalsIgnoreCase ("off")) return 0.0;
        return t.retainCharacters ("0123456789.").getDoubleValue() / 100.0;
    };
    addAndMakeVisible (limiterLabel);
    addAndMakeVisible (limiterSlider);

    limiterValueLabel.getProperties().set ("customFont", true);
    limiterValueLabel.setFont (juce::Font (26.0f, juce::Font::bold));
    limiterValueLabel.setInterceptsMouseClicks (false, false);
    limiterValueLabel.setJustificationType (juce::Justification::centred);
    limiterValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (limiterValueLabel);

    limiterSlider.onValueChange = [this] {
        const double v = limiterSlider.getValue();
        limiterValueLabel.setText (v <= 0.0 ? "Off"
                                             : juce::String (juce::roundToInt (v * 100)) + "%",
                                   juce::dontSendNotification);
    };
    limiterSlider.onValueChange();

    limiterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::limiterAmount, limiterSlider);

    // Voice count: 1 (mono) .. 16 (poly).
    for (int i = 1; i <= 16; ++i) voiceCountBox.addItem (juce::String (i), i);
    addAndMakeVisible (voiceCountLabel);
    addAndMakeVisible (voiceCountBox);
    voiceCountAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::voiceCount, voiceCountBox);

    // Pitch-bend range.
    for (int i = 1; i <= 24; ++i) pbRangeBox.addItem (juce::String (i), i);
    addAndMakeVisible (pbRangeLabel);
    addAndMakeVisible (pbRangeBox);
    pbRangeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::pitchBendRange, pbRangeBox);

    // Slice Start — MIDI note names, middle C = C3 convention (octave param 3).
    for (int i = 0; i < 128; ++i)
        sliceStartBox.addItem (juce::MidiMessage::getMidiNoteName (i, true, true, 3), i + 1);
    addAndMakeVisible (sliceStartLabel);
    addAndMakeVisible (sliceStartBox);
    sliceStartAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::slicerLoNote, sliceStartBox);

    // Slices — integer count 1–128.
    for (int i = 1; i <= 128; ++i)
        sliceCountBox.addItem (juce::String (i), i);
    addAndMakeVisible (sliceCountLabel);
    addAndMakeVisible (sliceCountBox);
    sliceCountAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::sliceCount, sliceCountBox);

    // Portamento / Legato knob — same rotary style as the other knobs in the group.
    portamentoSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    portamentoSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    portamentoSlider.getProperties().set ("semitoneKnob", true);
    portamentoSlider.setRange (0.0, 1000.0, 0.1);
    portamentoSlider.textFromValueFunction = [] (double v) {
        if (v <= 0.0) return juce::String ("Off");
        return juce::String (juce::roundToInt (v)) + " ms";
    };
    portamentoSlider.valueFromTextFunction = [] (const juce::String& t) {
        if (t.equalsIgnoreCase ("off")) return 0.0;
        return t.retainCharacters ("0123456789.").getDoubleValue();
    };
    portamentoSlider.setTooltip (
        "Legato - pitch glide time in ms.\n\n"
        "When set above 0 ms the sampler enters legato mode:\n"
        "holding a note and pressing another glides pitch\n"
        "without restarting the envelope or sample position.\n\n"
        "Legato only engages while a note is physically held.\n"
        "If a note is released first, a new note-on starts fresh\n"
        "even if the release tail is still audible.");
    addAndMakeVisible (portamentoLabel);
    addAndMakeVisible (portamentoSlider);

    portamentoValueLabel.getProperties().set ("customFont", true);
    portamentoValueLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    portamentoValueLabel.setInterceptsMouseClicks (false, false);
    portamentoValueLabel.setJustificationType (juce::Justification::centred);
    portamentoValueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (portamentoValueLabel);

    portamentoSlider.onValueChange = [this] {
        const double v = portamentoSlider.getValue();
        portamentoValueLabel.setText (v <= 0.0 ? "Off"
                                                : juce::String (juce::roundToInt (v)),
                                      juce::dontSendNotification);
    };
    portamentoSlider.onValueChange();

    portamentoAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::portamento, portamentoSlider);

    apvts.addParameterListener (ParamIDs::slicerEnabled, this);

    // Set initial enabled state so controls reflect the restored parameter value.
    const bool slicerOn = apvts.getRawParameterValue (ParamIDs::slicerEnabled)->load() > 0.5f;
    sliceStartLabel.setEnabled (slicerOn);
    sliceStartBox  .setEnabled (slicerOn);
    sliceCountLabel.setEnabled (slicerOn);
    sliceCountBox  .setEnabled (slicerOn);
}

SampleControlPanel::~SampleControlPanel()
{
    apvts.removeParameterListener (ParamIDs::slicerEnabled, this);
}

void SampleControlPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == ParamIDs::slicerEnabled)
    {
        const bool on = newValue > 0.5f;
        // parameterChanged is called on the message thread, so direct UI calls are safe.
        sliceStartLabel.setEnabled (on);
        sliceStartBox  .setEnabled (on);
        sliceCountLabel.setEnabled (on);
        sliceCountBox  .setEnabled (on);
    }
}

void SampleControlPanel::setLimiterGainReductionSource (std::atomic<float>* src)
{
    limiterGrSource = src;
    startTimerHz (30);
}

void SampleControlPanel::RootNoteBlinkOverlay::paint (juce::Graphics& g)
{
    if (highlight)
    {
        g.setColour (SuomiLookAndFeel::kKnobPointer.withAlpha (0.5f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    }
}

void SampleControlPanel::blinkRootNoteBox()
{
    // 5 blinks × (7 on + 7 off) = 70 ticks at 30 Hz ≈ 2.3 s.
    rootNoteBlinkTicks = 70;

    // Create a floating overlay on the top-level window so it renders
    // above everything, including the tune-result dim layer.
    auto* topLevel = getTopLevelComponent();
    if (topLevel == nullptr)
        return;

    blinkOverlay = std::make_unique<RootNoteBlinkOverlay>();
    blinkOverlay->setInterceptsMouseClicks (false, false);
    auto boxBounds = topLevel->getLocalArea (&rootNoteBox, rootNoteBox.getLocalBounds());
    blinkOverlay->setBounds (boxBounds.expanded (2));
    topLevel->addAndMakeVisible (*blinkOverlay);
    blinkOverlay->toFront (false);
}

void SampleControlPanel::timerCallback()
{
    // Root-note blink animation: toggle highlight every 7 ticks (~230 ms per phase).
    if (rootNoteBlinkTicks > 0)
    {
        --rootNoteBlinkTicks;
        const bool hl = ((rootNoteBlinkTicks / 7) % 2) == 1;

        if (blinkOverlay != nullptr)
        {
            blinkOverlay->highlight = hl;
            blinkOverlay->toFront (false);
            blinkOverlay->repaint();
        }

        if (rootNoteBlinkTicks == 0)
            blinkOverlay.reset();
    }

    if (limiterGrSource == nullptr)
        return;

    const float gr = limiterGrSource->load (std::memory_order_relaxed);

    // Rise immediately, decay slowly (~0.95 per tick at 30 Hz ≈ 2 s to fall to noise floor).
    if (gr > displayedGrDb)
        displayedGrDb = gr;
    else
        displayedGrDb *= 0.95f;

    const float prev = static_cast<float> (
        static_cast<double> (limiterSlider.getProperties().getWithDefault ("limiterGrDb", 0.0)));

    if (std::abs (displayedGrDb - prev) > 0.05f)
    {
        limiterSlider.getProperties().set ("limiterGrDb", static_cast<double> (displayedGrDb));
        limiterSlider.repaint();
    }
}

void SampleControlPanel::resized()
{
    const int h = getHeight();
    constexpr int comboH = 22;
    constexpr int lblH   = 13;
    constexpr int gap    = 4;

    auto area = getLocalBounds().reduced (2);
    area.removeFromLeft (4);  // total 6 px from left edge, matching the step controller panel gap

    // ── Col 1: root note / sample direction / loop mode (stacked, no labels) ──
    const int col1W = 90;
    {
        auto col = area.removeFromLeft (col1W);
        const int tripleH = 3 * comboH + 2 * gap;
        const int topY    = (h - tripleH) / 2;
        rootNoteBox .setBounds (col.getX(), topY,                      col1W, comboH);
        sampleDirBox.setBounds (col.getX(), topY + comboH + gap,       col1W, comboH);
        loopModeBox .setBounds (col.getX(), topY + 2 * (comboH + gap), col1W, comboH);
    }
    area.removeFromLeft (gap);

    // ── Col 2: Voices + Pitch Bend (full-name label above each combo) ─────────
    const int col2W = 64;
    {
        auto col       = area.removeFromLeft (col2W);
        const int pairH = lblH + comboH;
        const int topY  = (h - (2 * pairH + gap)) / 2;
        voiceCountLabel.setBounds (col.getX(), topY,                      col2W, lblH);
        voiceCountBox  .setBounds (col.getX(), topY + lblH,               col2W, comboH);
        pbRangeLabel   .setBounds (col.getX(), topY + pairH + gap,        col2W, lblH);
        pbRangeBox     .setBounds (col.getX(), topY + pairH + gap + lblH, col2W, comboH);
    }
    area.removeFromLeft (gap);

    // ── Col 3: Slice Start key + Slices count (full-name label above each combo) ──
    const int col3W = 64;
    {
        auto col       = area.removeFromLeft (col3W);
        const int pairH = lblH + comboH;
        const int topY  = (h - (2 * pairH + gap)) / 2;
        sliceStartLabel.setBounds (col.getX(), topY,                      col3W, lblH);
        sliceStartBox  .setBounds (col.getX(), topY + lblH,               col3W, comboH);
        sliceCountLabel.setBounds (col.getX(), topY + pairH + gap,        col3W, lblH);
        sliceCountBox  .setBounds (col.getX(), topY + pairH + gap + lblH, col3W, comboH);
    }
    area.removeFromLeft (gap * 2);

    // ── Semitone | Legato | Stereo | Limiter | Gain knobs — equal-width, side by side ──
    {
        constexpr int knobGap = 8;
        // Five equal-width square knobs with gaps between them.
        const int maxKnobW = juce::jmin (SuomiLookAndFeel::kStdKnobSize,
                                         juce::jmin (h - lblH - 6,
                                                     (area.getWidth() - 4 * knobGap) / 5 - 2));
        const int knobW   = juce::jmax (24, maxKnobW);
        const int totalH  = lblH + 2 + knobW;
        const int topY    = (h - totalH) / 2;
        const int groupW  = 5 * knobW + 4 * knobGap;
        const int x0      = area.getCentreX() - groupW / 2;
        const int x1      = x0 + knobW + knobGap;
        const int x2      = x1 + knobW + knobGap;
        const int x3      = x2 + knobW + knobGap;
        const int x4      = x3 + knobW + knobGap;

        semitoneLabel         .setBounds (x0, topY,            knobW, lblH);
        semitoneSlider        .setBounds (x0, topY + lblH + 2, knobW, knobW);
        semitoneModIndicator  .setBounds (semitoneSlider.getBounds());
        semitoneValueLabel    .setBounds (x0, topY + lblH + 2, knobW, knobW);

        portamentoLabel    .setBounds (x1, topY,            knobW, lblH);
        portamentoSlider   .setBounds (x1, topY + lblH + 2, knobW, knobW);
        portamentoValueLabel.setBounds(x1, topY + lblH + 2, knobW, knobW);

        stereoLabel      .setBounds (x2, topY,            knobW, lblH);
        stereoSlider     .setBounds (x2, topY + lblH + 2, knobW, knobW);
        stereoGoniometer .setBounds (x2, topY + lblH + 2, knobW, knobW);

        limiterLabel       .setBounds (x3, topY,            knobW, lblH);
        limiterSlider      .setBounds (x3, topY + lblH + 2, knobW, knobW);
        limiterValueLabel  .setBounds (x3, topY + lblH + 2, knobW, knobW);

        gainLabel          .setBounds (x4, topY,            knobW, lblH);
        gainSlider         .setBounds (x4, topY + lblH + 2, knobW, knobW);
        gainValueLabel     .setBounds (x4, topY + lblH + 2, knobW, knobW);
    }
}

void SampleControlPanel::setStereoFieldSource (StereoFieldSource* source)
{
    stereoGoniometer.setSource (source);
}

void SampleControlPanel::setLfoModSource (std::atomic<float>* lfoOutput, std::atomic<float>* lfoDepth)
{
    semitoneModIndicator.setDestinationRange (-48.0f, 48.0f);
    semitoneModIndicator.setCenterSource (apvts.getRawParameterValue (ParamIDs::semitoneOffset));
    semitoneModIndicator.setSource (lfoOutput, lfoDepth);
}
