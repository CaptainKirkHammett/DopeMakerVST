#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LfoModIndicator.h"

class PhaserPanel : public juce::Component
{
public:
    explicit PhaserPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint   (juce::Graphics&) override;
    void resized () override;

    void setModSources (std::atomic<float>* gateLevel,    std::atomic<float>* gateAmt,
                        std::atomic<float>* afEnvLevel,  std::atomic<float>* afEnvAmt,
                        std::atomic<float>* mixBase,     std::atomic<float>* fbBase,
                        std::atomic<float>* lfoOutput,   std::atomic<float>* depthBase,
                        std::atomic<float>* centreFreqBase);

private:
    juce::ToggleButton enableButton { "Phaser" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::Label  rateLabel     { {}, "LFO Rate" };
    juce::Slider rateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;

    juce::Label  depthLabel    { {}, "Depth" };
    juce::Slider depthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach;

    juce::Label  fbLabel       { {}, "FB" };
    juce::Slider fbSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fbAttach;

    juce::Label    stagesLabel { {}, "Stages" };
    juce::ComboBox stagesBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stagesAttach;

    juce::Label  centreLabel   { {}, "Centre" };
    juce::Slider centreSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> centreAttach;

    juce::Label  mixLabel      { {}, "Mix" };
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;

    juce::Label  envAmtLabel  { {}, "Step" };
    juce::Slider envAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envAmtAttach;
    LfoModIndicator stepModIndicator;

    juce::Label  afEnvAmtLabel  { {}, "Env" };
    juce::Slider afEnvAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> afEnvAmtAttach;
    LfoModIndicator envModIndicator;
    LfoModIndicator lfoModIndicator;   // Phaser LFO → Centre Freq knob
};
