#include "EnvelopePanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

EnvelopePanel::EnvelopePanel (juce::AudioProcessorValueTreeState& apvts,
                               const juce::String& paramPrefix,
                               bool showDepth)
    : hasDepth (showDepth)
{
    titleLabel.setText (paramPrefix == "vol" ? "Volume Envelope" : "Pitch Envelope",
                        juce::dontSendNotification);
    titleLabel.getProperties().set ("customFont", true);
    titleLabel.setFont (juce::Font (21.13f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id) {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (sl);
        addAndMakeVisible (lb);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    attackAttach  = makeKnob (attackSlider,  attackLabel,  paramPrefix + "Attack");
    decayAttach   = makeKnob (decaySlider,   decayLabel,   paramPrefix + "Decay");
    sustainAttach = makeKnob (sustainSlider, sustainLabel, paramPrefix + "Sustain");
    releaseAttach = makeKnob (releaseSlider, releaseLabel, paramPrefix + "Release");

    addAndMakeVisible (releaseModIndicator);

    if (showDepth)
    {
        depthAttach = makeKnob (depthSlider, depthLabel, ParamIDs::pitchEnvDepth);
    }
}

void EnvelopePanel::setReleaseModSource (std::atomic<float>* lfoOutput,
                                         std::atomic<float>* lfoDepth,
                                         std::atomic<float>* baseRelease)
{
    releaseModIndicator.setModType (LfoModIndicator::ModType::MultiplicativeBipolar);
    releaseModIndicator.setNegativeBoost (1.5f);  // match audio-thread asymmetric boost
    releaseModIndicator.setBaseSource (baseRelease);
    // Vol release range: 0–10000 ms with skewFactor 0.3 (same as the APVTS param).
    releaseModIndicator.setNormalisableRange (
        juce::NormalisableRange<float> (0.0f, 10000.0f, 0.0f, 0.3f));
    releaseModIndicator.setSource (lfoOutput, lfoDepth);
}

void EnvelopePanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void EnvelopePanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    titleLabel.setBounds (area.removeFromTop (14));
    area.removeFromTop (3);

    constexpr int lblH   = 13;
    const int     numKnobs = 4 + (hasDepth ? 1 : 0);

    // Use forced size when the editor has synced knob sizes across panels; otherwise
    // fill available width proportionally, capped at the standard size.
    const int knobW = (forcedKnobSize > 0)
                    ? forcedKnobSize
                    : juce::jmin (SuomiLookAndFeel::kStdKnobSize,
                                  juce::jmax (30, area.getWidth() / numKnobs - 4));

    int knobX = area.getX();
    const int rowY = area.getY();

    auto addKnob = [&] (juce::Label& lb, juce::Slider& sl) {
        lb.setBounds (knobX, rowY,            knobW, lblH);
        sl.setBounds (knobX, rowY + lblH + 1, knobW, knobW);
        knobX += knobW + 4;
    };

    addKnob (attackLabel,  attackSlider);
    addKnob (decayLabel,   decaySlider);
    addKnob (sustainLabel, sustainSlider);
    addKnob (releaseLabel, releaseSlider);
    releaseModIndicator.setBounds (releaseSlider.getBounds());

    if (hasDepth)
        addKnob (depthLabel, depthSlider);
}
