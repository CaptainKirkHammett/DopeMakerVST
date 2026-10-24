#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Side-panel that sits to the right of the GateControllerPanel.
// Provides tempo-synced sidechain-style volume modulation with:
//   • Mode selector  (Off / Pre-FX / Post-FX / FX Only)
//   • Time division  (same grid as the gate / delay modules)
//   • Mix, Pre-delay, and Curve knobs
//   • Live envelope-curve preview / freehand draw mode
class SidechainPanel : public juce::Component
{
public:
    explicit SidechainPanel (juce::AudioProcessorValueTreeState& apvts);

    void paint      (juce::Graphics&) override;
    void resized    () override;
    void mouseMove  (const juce::MouseEvent&) override;
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseDrag  (const juce::MouseEvent&) override;
    void mouseUp    (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

    // Set the drawn envelope table owned by the processor.
    void setDrawnEnvelopeTable (float* table, int tableSize);

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

    // ── Visualization / draw ────────────────────────────────────────────────
    juce::Rectangle<int> vizArea;   // populated in resized(); read in paint()

    void drawEnvelopeViz (juce::Graphics&) const;

    // ── Draw-mode state ──────────────────────────────────────────────────────
    float* drawnTable     { nullptr };
    int    drawnTableSize { 0 };
    bool   isDrawing      { false };   // true while mouse is held inside vizArea
    bool   hasDrawnOnce   { false };  // true after user has drawn at least once
    int    lastDrawX      { -1 };      // previous pixel X for interpolation
    int    lastDrawY      { -1 };      // previous pixel Y for interpolation
    bool   hoveringViz    { false };   // true when cursor is over vizArea

    // "Knob Mode" button — shown on top of shape knobs when draw mode is on.
    juce::TextButton knobModeButton { "Knob Mode" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> drawModeAttach;

    bool isDrawMode() const;
    void applyDrawModeUI();
    void paintToTable (int pixelX0, int pixelY0, int pixelX1, int pixelY1);
    void smoothDrawnTable();
    void copyKnobEnvelopeToTable();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainPanel)
};
