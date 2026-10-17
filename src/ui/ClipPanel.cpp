#include "ClipPanel.h"
#include "../ParameterIDs.h"

ClipPanel::ClipPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::clipEnabled, enableButton);

    modeBox.addItem ("Hard", 1);
    modeBox.addItem ("Soft", 2);
    addAndMakeVisible (modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::clipMode, modeBox);

    amtSlider.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
    amtSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    amtSlider.getProperties().set ("fxKnob", true);
    amtSlider.getProperties().set ("centreTextKnob", juce::var ("Amt"));
    addAndMakeVisible (amtLabel);
    addAndMakeVisible (amtSlider);
    amtLabel.setVisible (false);
    amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::clipAmt, amtSlider);

    makeupSlider.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
    makeupSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    makeupSlider.getProperties().set ("fxKnob", true);
    makeupSlider.getProperties().set ("centreTextKnob", juce::var ("Mkup"));
    addAndMakeVisible (makeupSlider);
    makeupAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::clipMakeup, makeupSlider);
}

void ClipPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void ClipPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    // Controls are centered vertically in whatever height this panel has.
    enableButton.setBounds (area.removeFromLeft (72).withSizeKeepingCentre (72, 24));
    area.removeFromLeft (4);
    modeBox.setBounds (area.removeFromLeft (74).withSizeKeepingCentre (74, 24));
    area.removeFromLeft (4);
    constexpr int knobSz = 44;
    constexpr int slotW  = 52;
    amtSlider   .setBounds (area.removeFromLeft (slotW).withSizeKeepingCentre (knobSz, knobSz));
    area.removeFromLeft (4);
    makeupSlider.setBounds (area.removeFromLeft (slotW).withSizeKeepingCentre (knobSz, knobSz));
}
