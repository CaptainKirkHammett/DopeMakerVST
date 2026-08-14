#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class ClipPanel : public juce::Component
{
public:
    explicit ClipPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    juce::ToggleButton enableButton { "Clip" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;

    juce::Label  amtLabel  { {}, "Amt" };
    juce::Slider amtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach;

    juce::Slider makeupSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> makeupAttach;
};
