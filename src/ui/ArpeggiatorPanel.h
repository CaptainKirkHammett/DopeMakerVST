#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class ArpeggiatorPanel : public juce::Component
{
public:
    explicit ArpeggiatorPanel (juce::AudioProcessorValueTreeState& apvts);

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    // Top row: enable toggle + time division combo
    juce::ToggleButton enableButton { "Arpeggiator" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::ComboBox divBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttach;

    // Bottom row: four rotary knobs
    juce::Label  octLabel { {}, "Oct" };
    juce::Slider octSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octAttach;

    juce::Label  dirLabel { {}, "Dir" };
    juce::Slider dirSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dirAttach;

    juce::Label  noteLenLabel { {}, "Len" };
    juce::Slider noteLenSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noteLenAttach;

    juce::Label  probLabel { {}, "Prob" };
    juce::Slider probSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> probAttach;

    juce::Label  tieLabel { {}, "Tie" };
    juce::Slider tieSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tieAttach;

    // LFO for note-length modulation — two stacked combos + depth knob
    juce::ComboBox lfoWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoWaveAttach;

    juce::ComboBox lfoDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoDivAttach;

    juce::Label  lfoDepthLabel { {}, "LenFO" };
    juce::Slider lfoDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArpeggiatorPanel)
};
