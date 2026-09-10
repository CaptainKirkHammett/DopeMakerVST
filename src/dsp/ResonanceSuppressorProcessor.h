#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

// Dynamic resonance suppressor — detects and attenuates frequency bands that
// protrude above their neighbors, taming harsh or ringy tones in real-time.
//
// IIR mode: 24 fixed bandpass filters span 100 Hz – 14 kHz (logarithmically spaced).
// FFT mode: 512-sample STFT with per-bin spectral peak detection and suppression.
//
// Single-knob control: amount 0 = off, >0 = increasingly aggressive suppression.
class ResonanceSuppressorProcessor
{
public:
    ResonanceSuppressorProcessor()  = default;
    ~ResonanceSuppressorProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::AudioBuffer<float>& buffer, float amount, bool fftMode);
    void reset();

    // Returns the latency in samples introduced by the current mode.
    int getLatencySamples (bool fftMode) const { return fftMode ? kFFTSize : 0; }

private:
    // ── IIR band-based mode ──────────────────────────────────────────────────
    static constexpr int kNumBands = 24;
    static constexpr float kMinFreq = 100.0f;
    static constexpr float kMaxFreq = 14000.0f;

    struct Band
    {
        juce::dsp::IIR::Filter<float> bpL;
        juce::dsp::IIR::Filter<float> bpR;
        float centreFreq   { 0.0f };
        float energy       { 0.0f };
        float energyAccum  { 0.0f };
        float reduction    { 0.0f };
    };

    std::array<Band, kNumBands> bands;
    double sampleRate { 44100.0 };

    float reductionAttackCoeff  { 0.0f };
    float reductionReleaseCoeff { 0.0f };

    void initBandFrequencies();
    void updateCoefficients();
    void processIIR (juce::AudioBuffer<float>& buffer, float amount);

    // ── FFT spectral mode ────────────────────────────────────────────────────
    static constexpr int kFFTOrder = 9;                      // 2^9 = 512
    static constexpr int kFFTSize  = 1 << kFFTOrder;         // 512
    static constexpr int kHopSize  = kFFTSize / 2;           // 256 (50% overlap)
    static constexpr int kNumBins  = kFFTSize / 2 + 1;      // 257

    juce::dsp::FFT fft { kFFTOrder };

    // Per-channel overlap-add state using a circular buffer.
    struct OlaChannel
    {
        std::array<float, kFFTSize> inputRing  {};   // circular input buffer
        std::array<float, kFFTSize> outputRing {};   // circular overlap-add accumulator
        int writePos   { 0 };                        // circular write/read position
        int hopCounter { 0 };                        // samples since last hop
    };

    std::array<OlaChannel, 2> olaChannels;
    std::array<float, kFFTSize> window {};

    // Smoothed per-bin gain from previous hop.
    std::array<float, kNumBins> prevBinGain {};

    void initWindow();
    void processFFT (juce::AudioBuffer<float>& buffer, float amount);
    void processFFTFrame (float* fftData, float amount);

    JUCE_DECLARE_NON_COPYABLE (ResonanceSuppressorProcessor)
};
