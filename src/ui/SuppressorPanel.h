#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class SuppressorPanel : public juce::Component
{
public:
    explicit SuppressorPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    juce::ToggleButton enableButton { "AutoEQ" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::ToggleButton fftButton { "FFT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> fftAttach;

    juce::Slider amtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach;
};
