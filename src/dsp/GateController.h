#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "LfoProcessor.h"

// Tempo-synced step sequencer that outputs a per-step amplitude [0, 1].
// Used as the modulation source for per-FX ENV amount parameters.
class GateController
{
public:
    static constexpr int kMaxSteps = 32;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setStepCount   (int count);                      // 1–32
    void setDivision    (SyncDivision div);
    void setStepAmplitude (int stepIndex, float amplitude); // 0-indexed

    // Advance by numSamples at the given BPM; returns the current step amplitude.
    float advance (int numSamples, double bpm);

    void reset();

    // Written on the audio thread; read by the UI for the playback indicator.
    std::atomic<int>   currentStepForUI      { 0 };
    std::atomic<float> currentStepLevelForUI { 0.0f };

private:
    double computeSamplesPerStep (double bpm) const;

    static constexpr double kGlideSeconds = 0.005; // 5 ms anti-click ramp

    double sampleRate    = 44100.0;
    int    stepCount     = 16;
    int    currentStep   = 0;
    double samplePosition = 0.0;
    SyncDivision division { SyncDivision::Div1_4 };

    std::array<float, kMaxSteps> stepAmplitudes;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutput;
};
