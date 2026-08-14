#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LfoModIndicator.h"
#include "DuckFaderButton.h"

class DelayPanel : public juce::Component
{
public:
    explicit DelayPanel (juce::AudioProcessorValueTreeState& apvts);
    void paint   (juce::Graphics&) override;
    void resized () override;

    void setModSources (std::atomic<float>* gateLevel,    std::atomic<float>* gateAmt,
                        std::atomic<float>* mixBase,
                        std::atomic<float>* fbLfoOutput,  std::atomic<float>* fbLfoDepth,
                        std::atomic<float>* fbCutoffBase,
                        std::atomic<float>* mixLfoOutput, std::atomic<float>* mixLfoDepth,
                        std::atomic<float>* duckLfoOutput, std::atomic<float>* duckLfoDepth);

private:
    juce::ToggleButton enableButton { "Delay" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::Label  timeLLabel  { {}, "L" };
    juce::Slider timeLSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeLAttach;

    juce::Label  timeRLabel  { {}, "R" };
    juce::Slider timeRSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> timeRAttach;

    juce::ToggleButton syncButton { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    // Duck is a fader (0–1 float), not a simple toggle.
    // DuckFaderButton owns the SliderAttachment internally.
    std::unique_ptr<DuckFaderButton> duckFader;

    juce::ComboBox timeLDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeLDivAttach;

    juce::ComboBox timeRDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> timeRDivAttach;

    juce::Label  fbLabel     { {}, "FB" };
    juce::Slider fbSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fbAttach;

    juce::Label  mixLabel    { {}, "Mix" };
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;

    juce::Label  envAmtLabel  { {}, "Step" };
    juce::Slider envAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> envAmtAttach;
    LfoModIndicator stepModIndicator;
    LfoModIndicator mixLfoModIndicator;   // Mix LFO → Mix knob
    LfoModIndicator fbCutoffModIndicator; // Delay Filter LFO → FB Cutoff knob
    LfoModIndicator duckModIndicator;     // Duck → Mix knob (downward arc when ducking)

    juce::TextButton killTailButton { "Kill Tail" };
    // No ButtonAttachment — click manually pulses the parameter so DAW automation
    // controls the value without fighting a plugin-side auto-reset.

    // Row-2 section group labels.
    juce::Label delayFilterLfoLabel { {}, "Delay Filter LFO" };
    juce::Label delayMixLfoLabel    { {}, "Delay Mix LFO" };

    // Feedback filter controls.
    juce::ComboBox fbFilterModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fbFilterModeAttach;

    juce::Label  fbCutoffLabel    { {}, "Cut" };
    juce::Slider fbCutoffSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fbCutoffAttach;

    juce::Label  fbResLabel       { {}, "Res" };
    juce::Slider fbResSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fbResAttach;

    // Feedback LFO controls.
    juce::ComboBox fbLfoWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fbLfoWaveAttach;

    juce::Label  fbLfoRateLabel   { {}, "Rate" };
    juce::Slider fbLfoRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fbLfoRateAttach;

    juce::Label  fbLfoDepthLabel  { {}, "Depth" };
    juce::Slider fbLfoDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fbLfoDepthAttach;

    // Mix LFO controls.
    juce::ToggleButton mixLfoSyncButton { "Mix LFO Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mixLfoSyncAttach;

    juce::ComboBox mixLfoDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mixLfoDivAttach;

    juce::ComboBox mixLfoWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mixLfoWaveAttach;

    juce::Label  mixLfoRateLabel  { {}, "Rate" };
    juce::Slider mixLfoRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixLfoRateAttach;

    juce::Label  mixLfoDepthLabel { {}, "Depth" };
    juce::Slider mixLfoDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixLfoDepthAttach;

};
