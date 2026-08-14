#include "EnvFollowerPanel.h"
#include "../ParameterIDs.h"

EnvFollowerPanel::EnvFollowerPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::envFollowerEnabled, enableButton);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id) {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob", true);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    attackAttach  = makeKnob (attackSlider,  attackLabel,  ParamIDs::envFollowerAttack);
    attackSlider .getProperties().set ("envKnob",        true);
    attackSlider .getProperties().set ("centreTextKnob", juce::var ("Atk"));
    attackLabel  .setVisible (false);

    releaseAttach = makeKnob (releaseSlider, releaseLabel, ParamIDs::envFollowerRelease);
    releaseSlider.getProperties().set ("envKnob",        true);
    releaseSlider.getProperties().set ("centreTextKnob", juce::var ("Rel"));
    releaseLabel .setVisible (false);
}

void EnvFollowerPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void EnvFollowerPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    enableButton.setBounds (area.removeFromLeft (130).withSizeKeepingCentre (130, 24));
    area.removeFromLeft (8);

    constexpr int knobW = 61;
    constexpr int gap   = 8;
    constexpr int lblH  = 14;

    auto addKnob = [&] (juce::Label& lb, juce::Slider& sl) {
        auto col = area.removeFromLeft (knobW);
        if (!sl.getProperties().contains ("centreTextKnob"))
        {
            lb.setBounds (col.removeFromTop (lblH));
            lb.setJustificationType (juce::Justification::centred);
        }
        constexpr int knobSz = 47;   // matches Highcut / Lowcut knob size
        sl.setBounds (col.withSizeKeepingCentre (knobSz, knobSz));
        area.removeFromLeft (gap);
    };

    addKnob (attackLabel,  attackSlider);
    addKnob (releaseLabel, releaseSlider);
}
