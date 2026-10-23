#pragma once

#include <juce_dsp/juce_dsp.h>

enum class LfoWaveform
{
    Sine, Triangle, Square, SawtoothUp, SawtoothDown,
    RandomSine,   // smoothly interpolated random
    RandomSquare  // stepped random
};

enum class SyncDivision
{
    Div1_32, Div1_16, Div1_8, Div1_8D,
    Div1_4, Div1_4D, Div1_2, Div1_2D,
    Div1_1, Div1_1D, Div2_1, Div1_64
};

class LfoProcessor
{
public:
    LfoProcessor() = default;

    void prepare (double sampleRate);

    void setWaveform (LfoWaveform waveform);
    void setRate (float hz);
    void setSyncedRate (double bpm, SyncDivision division);
    void setDepth (float zeroToOne);
    void setTempoSync (bool enabled) { tempoSync = enabled; }

    // Resets LFO phase to zero (call on note-on when tempo-sync is active).
    void resetPhase();

    // Sets the phase directly to an arbitrary position in [0, 1).
    // Does NOT trigger cycle-boundary callbacks — use for external transport sync.
    void forcePhase (double phase0to1) noexcept;

    // Returns the next modulation value in -1..+1 range.
    float getNextSample();

    // Advances the LFO by numSamples without producing output; used for
    // block-rate FX-chain LFOs where we only need one value per block.
    void advance (int numSamples);

    // Converts a SyncDivision to Hz at the given BPM.
    static float syncDivisionToHz (double bpm, SyncDivision div);

    // Current phase in [0, 1).
    double getPhase() const noexcept { return phase; }

private:
    float generateSample() const;
    void  advancePhase();
    void  checkCycleBoundary();

    double sampleRate  { 44100.0 };
    float  rateHz      { 1.0f };
    float  depth       { 0.0f };
    double phase       { 0.0 };   // 0..1
    bool   tempoSync   { false };

    LfoWaveform currentWaveform { LfoWaveform::Sine };

    // Random Sine: cosine-interpolated between two random values per cycle.
    float randomSinePrev { 0.0f };
    float randomSineNext { 0.0f };

    // Random Square: held value for the current cycle.
    float randomSquareHeld { 0.0f };

    juce::Random rng;
};
