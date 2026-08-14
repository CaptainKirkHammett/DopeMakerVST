#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "LfoProcessor.h"

// Tempo-synced volume modulation that mimics sidechain compression.
//
// Each beat period is split into two regions:
//   1. Pre-delay  [0 .. preDelay01)  — volume stays at unity.
//   2. Curve      [preDelay01 .. 1)  — envelope applies: env = 1 + curve*(1-t)^2
//                                      where t goes 0→1 through the curve region.
//
// With curve < 0 (e.g. −1): instant volume dip that recovers to unity (classic duck).
// With curve > 0 (e.g. +1): instant volume boost that returns to unity.
// With curve = 0:           no modulation regardless of other settings.
//
// Final applied gain = max(0, 1 + mix * (env − 1)).
class SidechainProcessor
{
public:
    SidechainProcessor()  = default;
    ~SidechainProcessor() = default;

    void prepare (double sampleRate);
    void reset   ();

    // Process the buffer in-place.  Call from the audio thread only.
    void process (juce::AudioBuffer<float>& buffer,
                  double       bpm,
                  SyncDivision division,
                  float        preDelay01,  // 0..1 — fraction of period muted before rise
                  float        rise01,      // 0.01..1 — fraction of remaining period used for rise
                  float        curve,       // 0..1 — rise shape (0=slow, 0.5=linear, 1=fast)
                  float        fall01,      // 0..1 — fall span at end of cycle (0=instant, 1=full span)
                  float        mix);        // 0..1 — dry/wet

private:
    double sampleRate  { 44100.0 };
    double phase       { 0.0 };   // 0..1 within one period

    // One-pole LP smoother on the envelope gain — kills clicks on abrupt value changes.
    float  gainSmoothed { 1.0f };
    float  smoothCoeff  { 0.0f };

    // Evaluate the raw envelope at the current phase.
    float computeEnvelope (float preDelay01, float rise01, float curve, float fall01) const;

    JUCE_DECLARE_NON_COPYABLE (SidechainProcessor)
};
