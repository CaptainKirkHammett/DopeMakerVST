#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

// Dynamic resonance suppressor — detects and attenuates frequency bands that
// protrude above their neighbors, taming harsh or ringy tones in real-time.
//
// 12 fixed bandpass filters span 300 Hz – 10 kHz (logarithmically spaced).
// Each band's energy is compared to its neighbors; bands that stick out are
// subtracted from the signal proportionally.
//
// Single-knob control: amount 0 = off, >0 = increasingly aggressive suppression.
class ResonanceSuppressorProcessor
{
public:
    ResonanceSuppressorProcessor()  = default;
    ~ResonanceSuppressorProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer, float amount);
    void reset();

private:
    static constexpr int kNumBands = 12;
    static constexpr float kMinFreq = 300.0f;
    static constexpr float kMaxFreq = 10000.0f;

    struct Band
    {
        juce::dsp::IIR::Filter<float> bpL;   // bandpass analysis + subtraction (left)
        juce::dsp::IIR::Filter<float> bpR;   // bandpass analysis + subtraction (right)
        float centreFreq   { 0.0f };
        float energy       { 0.0f };          // previous block's measured energy
        float energyAccum  { 0.0f };          // current block's energy accumulator
        float reduction    { 0.0f };          // smoothed reduction gain (0 = no cut)
    };

    std::array<Band, kNumBands> bands;
    double sampleRate { 44100.0 };

    // One-pole smoother coefficient for reduction gain changes.
    float reductionAttackCoeff  { 0.0f };
    float reductionReleaseCoeff { 0.0f };

    void initBandFrequencies();
    void updateCoefficients();

    JUCE_DECLARE_NON_COPYABLE (ResonanceSuppressorProcessor)
};
