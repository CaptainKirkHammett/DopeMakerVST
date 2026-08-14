#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LfoModIndicator.h"

// Reusable ADSR panel.  Pass paramPrefix = "vol" or "pitch".
// showDepth = true adds a Depth knob (pitch envelope only).
// forceKnobSize() lets the editor synchronise knob sizes across panels.
class EnvelopePanel : public juce::Component
{
public:
    EnvelopePanel (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramPrefix,
                   bool showDepth = false);

    // Call before setBounds to lock all knobs to the same px size.
    void forceKnobSize (int px) { forcedKnobSize = px; }

    // Wire up the Vol Release LFO modulation indicator on the Release knob.
    // lfoOutput: raw LFO output in [-1,+1].  lfoDepth: volRelLfoDepth param (0–1).
    // baseRelease: volRelease param (ms).
    void setReleaseModSource (std::atomic<float>* lfoOutput,
                              std::atomic<float>* lfoDepth,
                              std::atomic<float>* baseRelease);

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    juce::Label  titleLabel;

    juce::Label  attackLabel  { {}, "A" };
    juce::Slider attackSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;

    juce::Label  decayLabel   { {}, "D" };
    juce::Slider decaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttach;

    juce::Label  sustainLabel { {}, "S" };
    juce::Slider sustainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttach;

    juce::Label  releaseLabel { {}, "R" };
    juce::Slider releaseSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;

    // Pitch envelope only.
    juce::Label  depthLabel   { {}, "Depth" };
    juce::Slider depthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach;

    bool hasDepth;
    int  forcedKnobSize { 0 };

    LfoModIndicator releaseModIndicator;
};
