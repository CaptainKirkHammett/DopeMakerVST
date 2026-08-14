#pragma once

#include <juce_dsp/juce_dsp.h>

// Tracks the amplitude envelope of an audio signal using one-pole attack/release smoothing.
// Feed the pre-FX buffer each block; the returned value drives FX parameter modulation.
class EnvelopeFollower
{
public:
    void prepare      (const juce::dsp::ProcessSpec& spec);
    void setAttackMs  (float ms);
    void setReleaseMs (float ms);

    // Returns the current envelope level in [0, 1] after processing the buffer.
    float process (const juce::AudioBuffer<float>& buffer);

private:
    void updateCoefficients();

    float sampleRate  = 44100.0f;
    float attackMs    = 10.0f;
    float releaseMs   = 200.0f;
    float attackCoef  = 0.0f;
    float releaseCoef = 0.0f;
    float envelope    = 0.0f;
};
