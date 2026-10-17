#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LfoModIndicator.h"
#include "GoniometerComponent.h"

// Compact sample-engine controls arranged in three columns plus a knob group:
//   Col 1 — root note / sample direction / loop mode (stacked combos)
//   Col 2 — Voices combo (with label) and Pitch Bend combo (with label)
//   Col 3 — Slice Start key selector and Slices count selector (with labels)
//   Knobs — Semitone | Stereo | Limiter | Gain | Legato (portamento), side by side
class SampleControlPanel : public juce::Component,
                           public juce::Timer,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SampleControlPanel (juce::AudioProcessorValueTreeState& apvts);
    ~SampleControlPanel() override;

    void resized() override;

    // Wire up the Pitch LFO modulation indicator on the Semitone knob.
    void setLfoModSource (std::atomic<float>* lfoOutput, std::atomic<float>* lfoDepth);

    // Wire up the goniometer overlay on the Stereo knob.
    void setStereoFieldSource (StereoFieldSource* source);

    // Wire up the limiter GR source so the Limiter knob pointer reflects clamping intensity.
    void setLimiterGainReductionSource (std::atomic<float>* src);

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& apvts;

    // Col 1 — no external labels; content is self-describing.
    juce::ComboBox rootNoteBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootNoteAttach;

    juce::ComboBox sampleDirBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sampleDirAttach;

    juce::ComboBox loopModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> loopModeAttach;

    // Col 2 — full-name labels above compact combos.
    juce::Label    voiceCountLabel { {}, "Voices" };
    juce::ComboBox voiceCountBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceCountAttach;

    juce::Label    pbRangeLabel { {}, "Pitch Bend" };
    juce::ComboBox pbRangeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pbRangeAttach;

    // Col 3 — slice start key and slice count selectors.
    juce::Label    sliceStartLabel { {}, "Slice Start" };
    juce::ComboBox sliceStartBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sliceStartAttach;

    juce::Label    sliceCountLabel { {}, "Slices" };
    juce::ComboBox sliceCountBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sliceCountAttach;

    // Legato / portamento — rotary knob in the main knob group; 0 = off.
    juce::Label  portamentoLabel { {}, "Legato" };
    juce::Slider portamentoSlider;
    juce::Label  portamentoValueLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> portamentoAttach;

    // Semitone — rotary knob; value label floats on top of the knob.
    juce::Label  semitoneLabel     { {}, "Semitone" };
    juce::Slider semitoneSlider;
    LfoModIndicator semitoneModIndicator;  // LFO indicator between slider and value label
    juce::Label  semitoneValueLabel;   // transparent overlay; mouse-transparent
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> semitoneAttach;

    // Gain (dB) — same style knob, placed next to semitone.
    juce::Label  gainLabel     { {}, "Gain" };
    juce::Slider gainSlider;
    juce::Label  gainValueLabel;       // transparent overlay; mouse-transparent
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;

    // Stereo width — placed between Semitone and Limiter.
    juce::Label  stereoLabel   { {}, "Stereo" };
    juce::Slider stereoSlider;
    GoniometerComponent stereoGoniometer; // replaces value label; overlays the knob face
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoAttach;

    // Limiter — placed next to Gain; 0 = off, 1 = max limiting.
    juce::Label  limiterLabel  { {}, "Limiter" };
    juce::Slider limiterSlider;
    juce::Label  limiterValueLabel;    // transparent overlay; mouse-transparent
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> limiterAttach;

    int semitoneKnobSize { 42 };

    std::atomic<float>* limiterGrSource  { nullptr };
    float               displayedGrDb   { 0.0f };   // decays slowly for smooth visual falloff
};
