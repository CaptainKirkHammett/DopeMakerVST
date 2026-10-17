#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "../dsp/GateController.h"
#include "DuckFaderButton.h"

class ParameterDisplay; // forward declaration — avoids circular include

// Step-sequencer panel: enable toggle, pattern length, time division, and 32 per-step
// amplitude sliders. The active step is highlighted via a timer-driven repaint.
class GateControllerPanel : public juce::Component,
                             private juce::Timer
{
public:
    GateControllerPanel (juce::AudioProcessorValueTreeState& apvts,
                         GateController& gate);
    ~GateControllerPanel() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // Wires the LCD display so that Max / Min / Rnd / Bin actions show a
    // descriptive status message instead of the last step's numeric value.
    void setParameterDisplay (ParameterDisplay* display);

private:
    void timerCallback() override;
    void setAllSteps (float normalizedValue);
    void randomizeSteps();
    void randomizeStepsBinary();
    void showActionMessage (const juce::String& message);

    juce::AudioProcessorValueTreeState& apvtsRef;
    GateController& gateRef;
    int             lastDisplayedStep { -1 };
    juce::Random    rng;
    ParameterDisplay* paramDisplay { nullptr };

    // ── Left controls ────────────────────────────────────────────────────────
    juce::ToggleButton enableButton { "Step Controller" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::Label  stepCountLabel { {}, "Steps" };
    juce::Slider stepCountSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepCountAttach;

    std::unique_ptr<DuckFaderButton> gateFader;
    juce::ComboBox divBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttach;

    juce::TextButton maxButton     { "Max" };
    juce::TextButton minButton     { "Min" };
    juce::TextButton randButton    { "Rnd" };
    juce::TextButton randBinButton { "Bin" };

    juce::Image              decorImage;
    juce::Rectangle<int>     decorImageArea;

    // ── Step sliders ─────────────────────────────────────────────────────────
    std::array<juce::Slider, GateController::kMaxSteps> stepSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,
               GateController::kMaxSteps> stepAttachments;
};
