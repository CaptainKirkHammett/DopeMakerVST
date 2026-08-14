#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LfoModIndicator.h"

class DistortionPanel : public juce::Component
{
public:
    explicit DistortionPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint   (juce::Graphics&) override;
    void resized () override;

    void setModSources (std::atomic<float>* gateLevel,  std::atomic<float>* gateAmt,
                        std::atomic<float>* afEnvLevel, std::atomic<float>* afEnvAmt,
                        std::atomic<float>* driveBase);

private:
    juce::ToggleButton enableButton { "Distortion" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;

    juce::Label  driveLabel  { {}, "Drive" };
    juce::Slider driveSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttach;

    juce::Label  outputLabel  { {}, "Output" };
    juce::Slider outputSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttach;

    juce::Label    bitDepthLabel { {}, "Bits" };
    juce::ComboBox bitDepthBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bitDepthAttach;

    juce::Label  bandLowLabel  { {}, "Lo" };
    juce::Slider bandLowSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bandLowAttach;

    juce::Label  bandMidLabel  { {}, "Mid" };
    juce::Slider bandMidSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bandMidAttach;

    juce::Label  bandHighLabel { {}, "Hi" };
    juce::Slider bandHighSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bandHighAttach;

    juce::Label  envAmtLabel  { {}, "Step" };
    juce::Slider envAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envAmtAttach;
    LfoModIndicator stepModIndicator;

    juce::Label  afEnvAmtLabel  { {}, "Env" };
    juce::Slider afEnvAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> afEnvAmtAttach;
};
