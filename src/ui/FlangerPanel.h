#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LfoModIndicator.h"

class FlangerPanel : public juce::Component
{
public:
    explicit FlangerPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint   (juce::Graphics&) override;
    void resized () override;

    void setModSources (std::atomic<float>* gateLevel,    std::atomic<float>* gateAmt,
                        std::atomic<float>* afEnvLevel,  std::atomic<float>* afEnvAmt,
                        std::atomic<float>* mixBase,     std::atomic<float>* feedbackBase,
                        std::atomic<float>* lfoDelayDelta, std::atomic<float>* lfoFullDepth,
                        std::atomic<float>* delayStartBase);

private:
    juce::ToggleButton enableButton { "Flanger" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::Label  rateLabel     { {}, "LFO Rate" };
    juce::Slider rateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;

    juce::Label  feedbackLabel      { {}, "Feedback" };
    juce::Slider feedbackSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttach;

    juce::Label  delayStartLabel   { {}, "LFO Start" };
    juce::Slider delayStartSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayStartAttach;

    juce::Label  delayEndLabel     { {}, "LFO End" };
    juce::Slider delayEndSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayEndAttach;

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
    LfoModIndicator lfoModIndicator;   // Flanger LFO → LFO Start knob (shows current delay sweep position)

    // Om symbol — occupies one knob slot, placed next to Mix.
    juce::Image omImage;
    juce::Image omScaled;
    juce::Rectangle<int> omBounds;

    // All-seeing eye — occupies the knob slot next to Om.
    juce::Image eyeImage;
    juce::Image eyeScaled;
    juce::Rectangle<int> eyeBounds;
};
