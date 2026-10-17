#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

enum class DistMode
{
    BitCrush,        // 0 — Bit Crush
    HardClip,        // 1 — Clip Hard
    SoftClip,        // 2 — Clip Soft  (cubic knee)
    Foldback,        // 3 — Foldback
    RingModulation,  // 4 — Ring Mod   (drive = frequency)
    HardSaturation,  // 5 — Sat Hard   (x / sqrt(1 + x²))
    SoftSaturation,  // 6 — Sat Soft   (tanh)
    Waveshaper       // 7 — Waveshaper (cubic Padé)
};

class DistortionProcessor
{
public:
    DistortionProcessor();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& context);
    void reset();

    void setMode       (DistMode mode);
    void setDrive      (float zeroToOne);   // 0–1
    void setOutputGain (float dB);          // -24–0
    void setBitDepth   (int bits);          // 2–16
    void setBandDrives (float low, float mid, float high); // 0–1 each

private:
    void rebuildShaper();
    void applyBandDrive (juce::AudioBuffer<float>& buf, int numSamples,
                         juce::SmoothedValue<float>& driveVal);

    using StereoIIR = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    // Crossover filters — fixed at 350 Hz (low/mid) and 3500 Hz (mid/high).
    StereoIIR lowLP;   // LP at 350 Hz  → low band
    StereoIIR midHP;   // HP at 350 Hz  → mid band (low cut)
    StereoIIR midLP;   // LP at 3500 Hz → mid band (high cut)
    StereoIIR highHP;  // HP at 3500 Hz → high band

    // Pre-allocated scratch buffers; resized in prepare().
    juce::AudioBuffer<float> lowBuf, midBuf, highBuf;

    juce::dsp::WaveShaper<float, std::function<float(float)>> shaper;
    juce::dsp::Gain<float>       outputGain;
    juce::SmoothedValue<float>   driveSmoothed;
    juce::SmoothedValue<float>   lowDriveSmoothed;
    juce::SmoothedValue<float>   midDriveSmoothed;
    juce::SmoothedValue<float>   highDriveSmoothed;

    DistMode currentMode  { DistMode::SoftSaturation };
    float    drive        { 0.5f };
    int      bitDepth     { 8 };

    // Ring modulation oscillator state (audio-thread only).
    float  ringModPhase      { 0.0f };
    double ringModSampleRate { 44100.0 };

    juce::dsp::ProcessSpec lastSpec { 44100.0, 512, 2 };
};
