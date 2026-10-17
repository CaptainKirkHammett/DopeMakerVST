#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>

#include "LfoProcessor.h"
#include "FilterProcessor.h"  // for FilterMode enum

class PingPongDelayProcessor
{
public:
    PingPongDelayProcessor();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& context);
    void reset();

    void setDelayTimes     (float leftMs, float rightMs);
    void setTempoSync      (bool enabled, double bpm,
                            SyncDivision leftDiv, SyncDivision rightDiv);
    void setFeedback       (float zeroToNinetyNine);     // 0–99
    void setMix            (float zeroToOne);

    void setFbFilterParams  (FilterMode mode, float cutoffHz, float resonance);
    void setFbLfoParams     (LfoWaveform waveform, float rateHz, float depth);
    void setMixLfoParams    (LfoWaveform waveform, float rateHz, float depth);

    // Duck amount (0.0 = off, 1.0 = maximum): input signal presence reduces the wet
    // mix by up to (0.5 * amount).  Uses a fast attack (~15 ms) / slow release
    // (~250 ms) AR envelope internally.
    void setDuckAmount (float zeroToOne);

    // Locks the mix LFO phase to the DAW transport.
    // Call every block (before process()) when tempo-sync is enabled and the
    // transport is playing.  ppqPosition is the start-of-block position in
    // quarter notes; rateHz is the already-resolved synced rate.
    void syncMixLfoToTransport (double ppqPosition, float rateHz, double bpm);

    // One-shot: zeroes the delay buffers on the next processBlock call.
    // Thread-safe: called from the UI thread.
    void killTail();

    // Raw (un-depth-scaled) LFO outputs in [-1, +1].
    // Written on the audio thread each block; read by the UI for mod indicators.
    std::atomic<float> fbLfoOutput  { 0.0f };
    std::atomic<float> mixLfoOutput { 0.0f };

    // Duck mod indicator feeds — written on the audio thread, read by the UI.
    // duckLfoOutput: negated smoothed input level (-1..0); duckLfoDepth: amount * 0.5 (0 when off).
    std::atomic<float> duckLfoOutput { 0.0f };
    std::atomic<float> duckLfoDepth  { 0.0f };

private:
    void applyKillTailIfRequested();
    void updateFbFilterCoefficients();

    using DelayLine = juce::dsp::DelayLine<float,
        juce::dsp::DelayLineInterpolationTypes::Linear>;

    DelayLine delayLineL, delayLineR;

    // Dedicated per-sample IIR filters for L and R feedback paths.
    // Using IIR::Filter directly (not FilterProcessor) because we need per-sample
    // processing inside the feedback loop; block-level processing would require
    // a second pass and complicates the cross-channel routing.
    juce::dsp::IIR::Filter<float> fbIirL, fbIirR;

    LfoProcessor fbLfo;
    LfoProcessor mixLfo;

    juce::SmoothedValue<float> feedbackSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    float baseMix { 0.35f };  // user-set dry/wet level; LFO offsets this in process()
    juce::SmoothedValue<float> delayTimeLSmoothed;   // in samples
    juce::SmoothedValue<float> delayTimeRSmoothed;

    std::atomic<bool> killRequested { false };

    // Depth is applied manually so the raw LFO output can be exposed for mod indicators.
    float fbLfoDepthCache  { 0.0f };
    float mixLfoDepthCache { 0.0f };

    double sampleRate       { 44100.0 };
    float  delayTimeLMs     { 250.0f };
    float  delayTimeRMs     { 500.0f };

    // Feedback filter parameters — stored so LFO can modulate the cutoff each block.
    FilterMode currentFbMode      { FilterMode::LowPass };
    float      baseFbCutoff       { 20000.0f };
    float      currentFbResonance { 0.707f };

    // Duck mode state.
    float duckAmount       { 0.0f };   // user-set amount 0..1; 0 = off
    float duckEnvLevel     { 0.0f };   // smoothed 0..1 envelope of input level
    float duckAttackCoeff  { 0.0f };   // per-sample one-pole attack coefficient
    float duckReleaseCoeff { 0.0f };   // per-sample one-pole release coefficient
};
