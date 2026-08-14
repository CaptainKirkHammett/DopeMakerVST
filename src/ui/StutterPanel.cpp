#include "StutterPanel.h"
#include "../ParameterIDs.h"
#include "../dsp/StutterProcessor.h"

StutterPanel::StutterPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts)
{
    // Purple toggle — same style as Step Controller / Granulizer (no fxButton property).
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::stutterEnabled, enableButton);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb,
                         const juce::String& paramId, const juce::String& centreText)
    {
        sl.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob",         true);
        sl.getProperties().set ("stutterKnob",    true);
        sl.getProperties().set ("centreTextKnob", juce::var (centreText));
        addChildComponent (lb);  // hidden — centreTextKnob draws label inside the knob
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramId, sl);
    };

    amountAttach   = makeKnob (amountSlider,   amountLabel,   ParamIDs::stutterAmount,     "Prob");
    lfoLowAttach   = makeKnob (lfoLowSlider,   lfoLowLabel,   ParamIDs::stutterLfoLow,   "From\n1/8");
    lfoHighAttach  = makeKnob (lfoHighSlider,  lfoHighLabel,  ParamIDs::stutterLfoHigh,  "To\n1/16");
    lfoSpeedAttach = makeKnob (lfoSpeedSlider, lfoSpeedLabel, ParamIDs::stutterLfoSpeed, "Spd");
    midiLenAttach  = makeKnob (midiLenSlider,  midiLenLabel,  ParamIDs::stutterMidiLength, "Len");

    // Len mode toggle — small button next to the Len knob.
    lenModeButton.setButtonText ("Len Alt");
    lenModeButton.getProperties().set ("smallFont", true);
    lenModeButton.setTooltip (
        "Len mode\n\n"
        "OFF (Gate): Len controls how much of each repeat is audible - "
        "the rest is silence. Repeat speed follows the time division.\n\n"
        "ON (Repeat): Len directly controls the loop size - "
        "shorter Len means a shorter loop and faster repetition.");
    addAndMakeVisible (lenModeButton);
    lenModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::stutterLenRepeat, lenModeButton);

    // Len LFO waveform combo.
    for (const auto& name : juce::StringArray { "Sine","Tri","Sqr","SawU","SawD","Rnd~","Rnd|" })
        lenLfoWaveBox.addItem (name, lenLfoWaveBox.getNumItems() + 1);
    addAndMakeVisible (lenLfoWaveBox);
    lenLfoWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::stutterLenLfoWaveform, lenLfoWaveBox);

    // Len LFO Amt and Spd knobs.
    lenLfoAmtAttach = makeKnob (lenLfoAmtSlider, lenLfoAmtLabel, ParamIDs::stutterLenLfoDepth, "LFO\nAmt");
    lenLfoSpdAttach = makeKnob (lenLfoSpdSlider, lenLfoSpdLabel, ParamIDs::stutterLenLfoRate,  "LFO\nRate");

    // Len LFO modulation indicator — overlays the Len knob.
    lenLfoModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    lenLfoModIndicator.setModType (LfoModIndicator::ModType::Additive);
    lenLfoModIndicator.setPointerColour (juce::Colour (0xffff8800));
    lenLfoModIndicator.setDestinationRange (0.0f, 1.0f);
    addAndMakeVisible (lenLfoModIndicator);

    // Division LED display — read-only, updated by timer.
    divDisplay.setText ("1/8", juce::dontSendNotification);
    divDisplay.setJustificationType (juce::Justification::centred);
    divDisplay.setFont (juce::Font (11.0f, juce::Font::bold));
    divDisplay.setColour (juce::Label::textColourId, juce::Colour (0xff00e5ff));
    divDisplay.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1a1a2e));
    divDisplay.setColour (juce::Label::outlineColourId, juce::Colour (0xff00e5ff).withAlpha (0.4f));
    divDisplay.setInterceptsMouseClicks (true, false);  // allow hover for tooltip
    divDisplay.setTooltip (
        "Current time division\n\n"
        "The LFO sweeps between the From and To knobs, picking a division each stutter step. "
        "Fast divisions (1/32, 1/64) are limited to one step each. "
        "They may appear back-to-back once (e.g. 1/32 then 1/64), "
        "but after two consecutive fast steps the next step is always "
        "a slower division (1/4, 1/8, or 1/16) chosen at random from the From/To range.");
    addAndMakeVisible (divDisplay);

    startTimerHz (30);
}

StutterPanel::~StutterPanel()
{
    stopTimer();
}

void StutterPanel::setEffectiveDivSource (std::atomic<int>* source)
{
    effectiveDivSource = source;
}

void StutterPanel::setLenLfoModSource (std::atomic<float>* lfoOutput,
                                        std::atomic<float>* lfoDepth)
{
    lenLfoModIndicator.setCenterSource (apvtsRef.getRawParameterValue (ParamIDs::stutterMidiLength));
    lenLfoModIndicator.setSource (lfoOutput, lfoDepth);
}

void StutterPanel::timerCallback()
{
    if (effectiveDivSource == nullptr)
        return;

    const int idx = juce::jlimit (0, StutterProcessor::kNumDivisions - 1,
        effectiveDivSource->load (std::memory_order_relaxed));

    if (idx != cachedDivIdx_)
    {
        cachedDivIdx_ = idx;
        divDisplay.setText (StutterProcessor::kDivisionNames[idx], juce::dontSendNotification);
    }

    // Update Low/High knob centre text: "From\n<div>" and "To\n<div>".
    const int lowIdx  = juce::jlimit (0, StutterProcessor::kNumDivisions - 1,
        static_cast<int> (apvtsRef.getRawParameterValue (ParamIDs::stutterLfoLow)->load()));
    const int highIdx = juce::jlimit (0, StutterProcessor::kNumDivisions - 1,
        static_cast<int> (apvtsRef.getRawParameterValue (ParamIDs::stutterLfoHigh)->load()));

    const juce::String lowText  = "From\n" + StutterProcessor::kDivisionNames[lowIdx];
    const juce::String highText = "To\n" + StutterProcessor::kDivisionNames[highIdx];

    if (lfoLowSlider.getProperties() ["centreTextKnob"].toString() != lowText)
    {
        lfoLowSlider.getProperties().set ("centreTextKnob", lowText);
        lfoLowSlider.repaint();
    }
    if (lfoHighSlider.getProperties() ["centreTextKnob"].toString() != highText)
    {
        lfoHighSlider.getProperties().set ("centreTextKnob", highText);
        lfoHighSlider.repaint();
    }
}

void StutterPanel::paint (juce::Graphics&)
{
    // No framing — sits directly on the main background like the Granulizer row.
}

void StutterPanel::resized()
{
    auto area = getLocalBounds().reduced (0, 4);

    constexpr int btnW       = 52;
    constexpr int btnH       = 22;
    constexpr int knobH      = 52;
    constexpr int lfoKnobH   = 46;
    constexpr int divLedW    = 34;
    constexpr int divLedH    = 20;
    constexpr int lenAltH    = 22;
    constexpr int mainKnobSz = 48;
    constexpr int waveComboW = 62;
    constexpr int waveComboH = 20;
    constexpr int rightPad   = 10;  // align waveform combo right edge with sidechain visual

    enableButton.setBounds (area.removeFromLeft (btnW).withSizeKeepingCentre (btnW, btnH));

    // Reserve waveform combo on the right with padding.
    area.removeFromRight (rightPad);
    auto waveArea = area.removeFromRight (waveComboW);
    lenLfoWaveBox.setBounds (waveArea.withSizeKeepingCentre (waveComboW, waveComboH));

    // Remaining area: 10 items distributed evenly.
    // Prob, DivLED, From, To, Spd, Len, LenAlt, LFO Amt, LFO Rate = 9 slots
    constexpr int numSlots = 9;
    const int slotW     = area.getWidth() / numSlots;
    const int lfoKnobSz = juce::jmin (slotW - 2, 44);

    // Prob knob
    {
        auto col = area.removeFromLeft (slotW);
        amountLabel.setBounds (col);
        amountSlider.setBounds (col.withSizeKeepingCentre (mainKnobSz, knobH));
    }

    // Division LED display
    {
        auto col = area.removeFromLeft (slotW);
        divDisplay.setBounds (col.withSizeKeepingCentre (divLedW, divLedH));
    }

    // From, To, Spd knobs
    juce::Slider* sliders[] = { &lfoLowSlider, &lfoHighSlider, &lfoSpeedSlider };
    juce::Label*  labels[]  = { &lfoLowLabel,  &lfoHighLabel,  &lfoSpeedLabel };

    for (int i = 0; i < 3; ++i)
    {
        auto col = area.removeFromLeft (slotW);
        labels[i]->setBounds (col);
        sliders[i]->setBounds (col.withSizeKeepingCentre (mainKnobSz, knobH));
    }

    // Len knob
    {
        auto col = area.removeFromLeft (slotW);
        midiLenLabel.setBounds (col);
        midiLenSlider.setBounds (col.withSizeKeepingCentre (mainKnobSz, knobH));
        lenLfoModIndicator.setBounds (midiLenSlider.getBounds());
    }

    // Len Alt toggle button
    {
        auto col = area.removeFromLeft (slotW);
        const int altW = juce::jmin (slotW - 4, 48);
        lenModeButton.setBounds (col.withSizeKeepingCentre (altW, lenAltH));
    }

    // ── Len LFO knobs — fill remaining two slots ────────────────────────────
    const int lfoRemaining = area.getWidth();
    const int lfoSlotW = lfoRemaining / 2;
    {
        auto col = area.removeFromLeft (lfoSlotW);
        lenLfoAmtLabel.setBounds (col);
        lenLfoAmtSlider.setBounds (col.withSizeKeepingCentre (lfoKnobSz, lfoKnobH));
    }
    {
        lenLfoSpdLabel.setBounds (area);
        lenLfoSpdSlider.setBounds (area.withSizeKeepingCentre (lfoKnobSz, lfoKnobH));
    }
}
