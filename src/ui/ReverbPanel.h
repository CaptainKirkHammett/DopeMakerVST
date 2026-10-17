#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "DuckFaderButton.h"
#include "LfoModIndicator.h"

class ReverbPanel : public juce::Component
{
public:
    explicit ReverbPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint   (juce::Graphics&) override;
    void resized () override;

    // Wire duck mod indicator to the reverb processor's duck atomics.
    void setDuckModSources (std::atomic<float>* mixBase,
                            std::atomic<float>* duckLfoOutput,
                            std::atomic<float>* duckLfoDepth);

private:
    juce::AudioProcessorValueTreeState& apvts;

    juce::ToggleButton enableButton { "Reverb" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    std::unique_ptr<DuckFaderButton> duckFader;

    juce::Slider roomSizeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> roomSizeAttach;

    juce::Slider dampingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampingAttach;

    juce::Slider widthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttach;

    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;

    LfoModIndicator duckModIndicator;

    juce::ToggleButton reverseButton { "Rev" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAttach;

    juce::Slider revLenSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> revLenAttach;

    juce::ToggleButton revSyncButton { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> revSyncAttach;

    juce::ComboBox revDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> revDivAttach;

    void updateSyncVisuals();
};
