#include "ReverbPanel.h"
#include "../ParameterIDs.h"

ReverbPanel::ReverbPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvts (apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::reverbEnabled, enableButton);

    duckFader = std::make_unique<DuckFaderButton> (apvts, ParamIDs::reverbDuck);
    addAndMakeVisible (*duckFader);

    auto makeKnob = [&] (juce::Slider& sl, const juce::String& id, const juce::String& label) {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob", true);
        sl.getProperties().set ("centreTextKnob", juce::var (label));
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    roomSizeAttach = makeKnob (roomSizeSlider, ParamIDs::reverbRoomSize, "Room");
    dampingAttach  = makeKnob (dampingSlider,  ParamIDs::reverbDamping,  "Damp");
    widthAttach    = makeKnob (widthSlider,    ParamIDs::reverbWidth,    "Width");
    mixAttach      = makeKnob (mixSlider,      ParamIDs::reverbMix,      "Mix");

    duckModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (duckModIndicator);

    addAndMakeVisible (reverseButton);
    reverseAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::reverbReverse, reverseButton);

    revLenAttach = makeKnob (revLenSlider, ParamIDs::reverbRevLen, "Len");

    addAndMakeVisible (revSyncButton);
    revSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::reverbRevSync, revSyncButton);

    revDivBox.addItem ("1/64", 12);
    const juce::StringArray divNames {
        "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
    };
    for (int i = 0; i < divNames.size(); ++i)
        revDivBox.addItem (divNames[i], i + 1);
    addAndMakeVisible (revDivBox);
    revDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::reverbRevDiv, revDivBox);

    revSyncButton.onStateChange = [this] { updateSyncVisuals(); };
    updateSyncVisuals();
}

void ReverbPanel::setDuckModSources (std::atomic<float>* mixBase,
                                     std::atomic<float>* duckLfoOutput,
                                     std::atomic<float>* duckLfoDepth)
{
    duckModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    duckModIndicator.setCenterSource (mixBase);
    duckModIndicator.setDestinationRange (0.0f, 1.0f);
    duckModIndicator.setSource (duckLfoOutput, duckLfoDepth);
}

void ReverbPanel::updateSyncVisuals()
{
    const bool synced = revSyncButton.getToggleState();

    revLenSlider.setAlpha   (synced ? 0.35f : 1.0f);
    revLenSlider.setEnabled (! synced);
    revLenSlider.repaint();

    revDivBox.setAlpha   (synced ? 1.0f : 0.35f);
    revDivBox.setEnabled (synced);
}

void ReverbPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void ReverbPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromLeft (2);

    enableButton.setBounds (area.removeFromLeft (100).withSizeKeepingCentre (100, 24));
    area.removeFromLeft (8);
    duckFader->setBounds (area.removeFromLeft (55).withSizeKeepingCentre (55, 24));
    area.removeFromLeft (6);

    constexpr int knobW = 61;
    constexpr int gap   = 2;

    auto addKnob = [&] (juce::Slider& sl) {
        sl.setBounds (area.removeFromLeft (knobW));
        area.removeFromLeft (gap);
    };

    addKnob (roomSizeSlider);
    addKnob (dampingSlider);
    addKnob (widthSlider);
    addKnob (mixSlider);

    // Duck mod indicator overlays the mix knob.
    duckModIndicator.setBounds (mixSlider.getBounds());

    area.removeFromLeft (6);
    reverseButton.setBounds (area.removeFromLeft (50).withSizeKeepingCentre (50, 24));
    area.removeFromLeft (4);
    addKnob (revLenSlider);
    area.removeFromLeft (2);
    revSyncButton.setBounds (area.removeFromLeft (55).withSizeKeepingCentre (55, 24));
    area.removeFromLeft (4);
    revDivBox.setBounds (area.removeFromLeft (70).withSizeKeepingCentre (70, 24));
}
