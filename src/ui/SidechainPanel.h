#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Side-panel that sits to the right of the GateControllerPanel.
// Provides tempo-synced sidechain-style volume modulation with:
//   • Mode selector  (Off / Pre-FX / Post-FX)
//   • Time division  (same grid as the gate / delay modules)
//   • Mix, Pre-delay, and Curve knobs
//   • Live envelope-curve preview
class SidechainPanel : public juce::Component
{
public:
    explicit SidechainPanel (juce::AudioProcessorValueTreeState& apvts);

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    juce::AudioProcessorValueTreeState& apvtsRef;

    // ── Mode & division ───────────────────────────────────────────────────────
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;

    juce::Label    divLabel { {}, "Div" };
    juce::ComboBox divBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttach;

    // ── Knobs ─────────────────────────────────────────────────────────────────
    juce::Label  mixLabel      { {}, "Mix" };
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;

    juce::Label  preDelayLabel { {}, "Pre" };
    juce::Slider preDelaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preDelayAttach;

    juce::Label  riseLabel     { {}, "Rise" };
    juce::Slider riseSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> riseAttach;

    juce::Label  curveLabel    { {}, "Curve" };
    juce::Slider curveSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> curveAttach;

    juce::Label  fallLabel     { {}, "Fall" };
    juce::Slider fallSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fallAttach;

    // ── Visualization ─────────────────────────────────────────────────────────
    juce::Rectangle<int> vizArea;   // populated in resized(); read in paint()

    void drawEnvelopeViz (juce::Graphics&) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainPanel)
};
