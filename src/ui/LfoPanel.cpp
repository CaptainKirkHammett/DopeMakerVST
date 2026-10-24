#include "LfoPanel.h"
#include "../ParameterIDs.h"

LfoPanel::LfoPanel (juce::AudioProcessorValueTreeState& apvts)
{
    const juce::StringArray waveNames {
        "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Rnd Sine", "Rnd Square"
    };
    for (int i = 0; i < waveNames.size(); ++i)
        waveformBox.addItem (waveNames[i], i + 1);
    addAndMakeVisible (waveformLabel);
    addAndMakeVisible (waveformBox);
    waveformAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::lfoWaveform, waveformBox);

    rateSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    rateSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible (rateLabel);
    addAndMakeVisible (rateSlider);
    rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::lfoRate, rateSlider);

    depthSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    depthSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible (depthLabel);
    addAndMakeVisible (depthSlider);
    depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::lfoDepth, depthSlider);

    addAndMakeVisible (syncButton);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::lfoTempoSync, syncButton);

    const juce::StringArray divNames {
        "1/64", "1/32", "1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2", "1/2D", "1/1", "1/1D", "2/1"
    };
    for (int i = 0; i < divNames.size(); ++i)
        syncDivBox.addItem (divNames[i], i + 1);
    addAndMakeVisible (syncDivLabel);
    addAndMakeVisible (syncDivBox);
    syncDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::lfoSyncDiv, syncDivBox);

    // Grey out Rate knob when Sync is active.
    auto syncVisual = [this]
    {
        const bool synced = syncButton.getToggleState();
        const float a = synced ? 0.35f : 1.0f;
        rateSlider.setAlpha (a);
        rateLabel .setAlpha (a);
        rateSlider.setEnabled (! synced);
        rateSlider.repaint();

        const float divA = synced ? 1.0f : 0.35f;
        syncDivBox  .setAlpha (divA);
        syncDivLabel.setAlpha (divA);
        syncDivBox  .setEnabled (synced);
    };
    syncButton.onStateChange = syncVisual;
    syncVisual();
}

void LfoPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    const int rowH = 24;
    const int gap  = 4;

    waveformLabel.setBounds (area.removeFromTop (rowH));
    area.removeFromTop (gap);
    waveformBox.setBounds (area.removeFromTop (rowH));
    area.removeFromTop (gap);

    auto row = area.removeFromTop (rowH * 2 + gap);
    rateLabel.setBounds  (row.removeFromLeft (40));
    rateSlider.setBounds (row.removeFromLeft (60));
    row.removeFromLeft (8);
    depthLabel.setBounds  (row.removeFromLeft (40));
    depthSlider.setBounds (row.removeFromLeft (60));

    area.removeFromTop (gap);
    syncButton.setBounds (area.removeFromTop (rowH));
    area.removeFromTop (gap);
    syncDivLabel.setBounds (area.removeFromTop (rowH));
    syncDivBox.setBounds   (area.removeFromTop (rowH));
}
