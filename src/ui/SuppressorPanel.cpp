#include "SuppressorPanel.h"
#include "../ParameterIDs.h"

SuppressorPanel::SuppressorPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::suppressorEnabled, enableButton);

    amtSlider.setSliderStyle  (juce::Slider::LinearHorizontal);
    amtSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (amtSlider);
    amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::suppressorAmt, amtSlider);
}

void SuppressorPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void SuppressorPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    enableButton.setBounds (area.removeFromLeft (72).withSizeKeepingCentre (72, 24));
    area.removeFromLeft (6);
    amtSlider.setBounds (area);
}
