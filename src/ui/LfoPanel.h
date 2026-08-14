#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class LfoPanel : public juce::Component
{
public:
    explicit LfoPanel (juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

private:
    juce::Label    waveformLabel   { {}, "Wave" };
    juce::ComboBox waveformBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttach;

    juce::Label  rateLabel         { {}, "Rate" };
    juce::Slider rateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;

    juce::Label  depthLabel        { {}, "Depth" };
    juce::Slider depthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach;

    juce::ToggleButton syncButton  { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    juce::Label    syncDivLabel    { {}, "Div" };
    juce::ComboBox syncDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncDivAttach;
};
