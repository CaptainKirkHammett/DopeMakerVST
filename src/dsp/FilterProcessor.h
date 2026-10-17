#pragma once

#include <juce_dsp/juce_dsp.h>

enum class FilterMode { LowPass, HighPass, BandPass, CombPlus, CombMinus };

class FilterProcessor
{
public:
    FilterProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& context);
    void reset();

    // Call once per block; recomputes coefficients only when values change.
    void setParameters (FilterMode mode, float cutoffHz, float resonance);

private:
    void updateCoefficients();
    void processComb (juce::dsp::ProcessContextReplacing<float>& context);

    using FilterType = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    FilterType filter;

    juce::dsp::ProcessSpec lastSpec { 44100.0, 512, 2 };

    FilterMode  currentMode    { FilterMode::LowPass };
    float       currentCutoff  { 20000.0f };
    float       currentQ       { 0.707f };
    bool        needsUpdate    { true };

    // Per-sample cutoff smoothing to prevent clicks from fast step modulation.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff { 20000.0f };
    bool        smoothingActive { false };

    // Comb filter delay line — allocated in prepare(), one buffer per channel.
    // Power-of-2 size covers down to ~3 Hz at 192 kHz.
    static constexpr int kCombBufSize = 16384;
    juce::AudioBuffer<float> combDelayBuf;
    int   combWritePos        { 0 };
    int   combDelaySamples    { 1 };     // integer target (for coefficient calc)
    float combFeedback        { 0.0f };

    // Per-sample smoothing for comb delay length — prevents clicks from
    // integer delay jumps when sweeping cutoff.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedCombDelay { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedCombFeedback { 0.0f };
};
