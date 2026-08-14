#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LfoModIndicator.h"

class FilterPanel : public juce::Component
{
public:
    explicit FilterPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint   (juce::Graphics&) override;
    void resized () override;

    void setModSources (std::atomic<float>* gateLevel,  std::atomic<float>* gateAmt,
                        std::atomic<float>* afEnvLevel, std::atomic<float>* afEnvAmt,
                        std::atomic<float>* cutoffBase,
                        std::atomic<float>* lfoOutput,  std::atomic<float>* lfoDepth);

private:
    juce::ToggleButton enableButton { "Filter" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;

    juce::Label  cutoffLabel  { {}, "Cutoff" };
    juce::Slider cutoffSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttach;

    juce::Label  resonanceLabel  { {}, "Res" };
    juce::Slider resonanceSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttach;

    juce::Label  lfoRateLabel  { {}, "LFO Rate" };
    juce::Slider lfoRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttach;

    juce::Label  lfoDepthLabel  { {}, "LFO Depth" };
    juce::Slider lfoDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoDepthAttach;

    juce::Label  lfoWaveLabel { {}, "LFO Wave" };
    juce::Slider lfoWaveSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoWaveAttach;

    juce::Label  envAmtLabel  { {}, "Step" };
    juce::Slider envAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envAmtAttach;
    LfoModIndicator stepModIndicator;
    LfoModIndicator lfoModIndicator;   // Filter LFO → Cutoff knob

    juce::Label  afEnvAmtLabel  { {}, "Env" };
    juce::Slider afEnvAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> afEnvAmtAttach;
};
