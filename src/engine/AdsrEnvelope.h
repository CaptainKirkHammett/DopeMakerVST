#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Thin wrapper around juce::ADSR that accepts millisecond timing values
// and converts them to the seconds-based juce::ADSR::Parameters struct.
class AdsrEnvelope
{
public:
    AdsrEnvelope() = default;

    void prepare (double sampleRate);

    // All timing values in milliseconds; sustainLevel in 0–1.
    void setParameters (float attackMs, float decayMs, float sustainLevel, float releaseMs);

    void noteOn();
    void noteOff();
    void reset();

    // Returns the next envelope sample in 0–1 range.
    float getNextSample();

    bool isActive() const;

private:
    juce::ADSR adsr;
    double     currentSampleRate { 44100.0 };
};
