#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "VelocityMeterComponent.h"

// Magixx group — Sample Start LFO, Volume Release LFO, Pitch LFO, and Velocity LFO controls.
// Knobs are kMagixxKnobSize (40% larger than the standard knob size).
// Rendered with a visible purple-tinted border and bold "Magixx" title.
class MagixxPanel : public juce::Component
{
public:
    explicit MagixxPanel (juce::AudioProcessorValueTreeState& apvts);

    void paint  (juce::Graphics&) override;
    void resized() override;

    // Wire up the last-played-velocity meter below the Prob knob.
    void setVelocitySource (std::atomic<float>*    lastVelocity,
                            std::atomic<uint32_t>* generation);

    // Returns the local bounds of the slot reserved for the granulizer overlay panel.
    // Valid after the first resized() call.  The slot sits below the Vol Release LFO
    // and Pitch LFO columns and spans both of them.
    juce::Rectangle<int> getGranulizerSlotBounds() const { return granulizerSlotBounds; }

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Cached column bounds — set in resized(), read by paint() for embossed panels.
    std::array<juce::Rectangle<int>, 4> columnBounds;

    // Bounds of the granulizer overlay slot (local coords).  Covers the bottom 50 px
    // of the Vol Release LFO + Pitch LFO columns combined.
    juce::Rectangle<int> granulizerSlotBounds;

    // ── Sample Start LFO ─────────────────────────────────────────────────────
    juce::Label    startLfoTitle   { {}, "Sample Start Modulation" };

    juce::Label    startWaveLabel  { {}, "Wave" };
    juce::ComboBox startWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> startWaveAttach;

    juce::ToggleButton syncButton  { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;

    juce::Label    syncDivLabel    { {}, "Div" };
    juce::ComboBox syncDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncDivAttach;

    juce::Label  startRateLabel   { {}, "Rate" };
    juce::Slider startRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> startRateAttach;

    juce::Label  startDepthLabel  { {}, "Depth" };
    juce::Slider startDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> startDepthAttach;

    // Override button — replaces LFO with step-controller amplitude for sample start.
    juce::ToggleButton stepOverrideButton { "Step" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stepOverrideAttach;

    // Slicer button — each MIDI note plays one evenly-spaced slice of the sample window.
    // Mutually exclusive with stepOverrideButton.
    juce::ToggleButton slicerButton { "Slicer" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slicerAttach;

    // ── Pitch LFO ─────────────────────────────────────────────────────────────
    juce::Label    pitchLfoTitle    { {}, "Pitch LFO" };

    juce::Label    pitchWaveLabel   { {}, "Wave" };
    juce::ComboBox pitchWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pitchWaveAttach;

    juce::ToggleButton pitchSyncButton { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pitchSyncAttach;

    juce::Label    pitchSyncDivLabel { {}, "Div" };
    juce::ComboBox pitchSyncDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pitchSyncDivAttach;

    juce::Label  pitchRateLabel    { {}, "Rate" };
    juce::Slider pitchRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchRateAttach;

    juce::Label  pitchDepthLabel   { {}, "Depth" };
    juce::Slider pitchDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchDepthAttach;

    // ── Velocity LFO ─────────────────────────────────────────────────────────
    juce::Label    velLfoTitle    { {}, "MIDI Velocity LFO" };

    juce::Label  velProbLabel  { {}, "Prob" };
    juce::Slider velProbSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velProbAttach;

    juce::Label    velWaveLabel   { {}, "Wave" };
    juce::ComboBox velWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> velWaveAttach;

    juce::ToggleButton velSyncButton { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> velSyncAttach;

    juce::Label    velSyncDivLabel { {}, "Div" };
    juce::ComboBox velSyncDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> velSyncDivAttach;

    juce::Label  velRateLabel    { {}, "Rate" };
    juce::Slider velRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velRateAttach;

    juce::Label  velDepthLabel   { {}, "Depth" };
    juce::Slider velDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> velDepthAttach;

    VelocityMeterComponent velMeter;

    // ── Volume Release LFO ────────────────────────────────────────────────────
    juce::Label    relLfoTitle     { {}, "Volume Envelope Release LFO" };

    juce::Label    relWaveLabel    { {}, "Wave" };
    juce::ComboBox relWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> relWaveAttach;

    juce::ToggleButton relSyncButton  { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> relSyncAttach;

    juce::Label    relSyncDivLabel { {}, "Div" };
    juce::ComboBox relSyncDivBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> relSyncDivAttach;

    juce::Label  relRateLabel     { {}, "Rate" };
    juce::Slider relRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> relRateAttach;

    juce::Label  relDepthLabel    { {}, "Depth" };
    juce::Slider relDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> relDepthAttach;
};
