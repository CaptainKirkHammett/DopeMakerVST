#pragma once

#include <juce_dsp/juce_dsp.h>

// Brick-wall limiter with a single "amount" control (0..1).
// As amount increases: threshold drops from 0 dB to -20 dB and a compensating
// output attenuation of up to -6 dB is applied, so raising the knob tightens
// peaks without increasing perceived loudness.
class LimiterProcessor
{
public:
    LimiterProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& context);
    void reset();

    // amount: 0 = bypass, 1 = maximum limiting. Call once per block.
    void setAmount (float amount);

    // Gain reduction applied by the brick-wall stage in the last processed block
    // (positive dB, 0 = no limiting). Written on the audio thread; read by the UI.
    std::atomic<float> gainReductionDb { 0.0f };

private:
    juce::dsp::Limiter<float> limiter;

    // Smoothed output compensation — avoids zipper noise on automation.
    juce::SmoothedValue<float> outputGainSmoothed;
    int lastBlockSize { 0 };

    // Stored so process() can compute GR without re-deriving it.
    float currentThresholdDb { 0.0f };
    float lastSetThresholdDb { 0.0f };

    // Per-sample EMA of input amplitude — used as the level signal for GR display.
    float avgInput { 0.0f };

    // Track previous amount so we can detect bypass→active transitions.
    float previousAmount { 0.0f };
};
