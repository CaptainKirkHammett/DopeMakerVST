#include "LfoProcessor.h"
#include <cmath>

void LfoProcessor::prepare (double sr)
{
    sampleRate = sr;
}

void LfoProcessor::setWaveform (LfoWaveform waveform)
{
    currentWaveform = waveform;
}

void LfoProcessor::setRate (float hz)
{
    const float newHz = juce::jlimit (0.01f, 20.0f, hz);
    if (std::abs (newHz - rateHz) < 1e-6f)
        return;

    rateHz = newHz;
}

void LfoProcessor::setSyncedRate (double bpm, SyncDivision division)
{
    rateHz = syncDivisionToHz (bpm, division);
}

void LfoProcessor::setDepth (float zeroToOne)
{
    depth = juce::jlimit (0.0f, 1.0f, zeroToOne);
}

void LfoProcessor::resetPhase()
{
    phase = 0.0;
    randomSinePrev = rng.nextFloat() * 2.0f - 1.0f;
    randomSineNext = rng.nextFloat() * 2.0f - 1.0f;
    randomSquareHeld = rng.nextFloat() * 2.0f - 1.0f;
}

void LfoProcessor::forcePhase (double phase0to1) noexcept
{
    phase = std::fmod (phase0to1, 1.0);
    if (phase < 0.0)
        phase = 0.0;
}

float LfoProcessor::getNextSample()
{
    const float value = generateSample() * depth;
    advancePhase();
    return value;
}

void LfoProcessor::advance (int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        generateSample();   // advance random states
        advancePhase();
    }
}

void LfoProcessor::advancePhase()
{
    phase += static_cast<double> (rateHz) / sampleRate;
    if (phase >= 1.0)
    {
        phase -= 1.0;
        checkCycleBoundary();
    }
}

void LfoProcessor::checkCycleBoundary()
{
    // Shift current target to previous, pick a new random target.
    // Values span the full -1..+1 range for natural variation.
    randomSinePrev = randomSineNext;
    randomSineNext = rng.nextFloat() * 2.0f - 1.0f;

    randomSquareHeld = rng.nextFloat() * 2.0f - 1.0f;
}

float LfoProcessor::generateSample() const
{
    const float p = static_cast<float> (phase);

    switch (currentWaveform)
    {
        case LfoWaveform::Sine:
            return std::sin (juce::MathConstants<float>::twoPi * p);

        case LfoWaveform::Triangle:
            return (p < 0.5f) ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);

        case LfoWaveform::Square:
            return (p < 0.5f) ? 1.0f : -1.0f;

        case LfoWaveform::SawtoothUp:
            return 2.0f * p - 1.0f;

        case LfoWaveform::SawtoothDown:
            return 1.0f - 2.0f * p;

        case LfoWaveform::RandomSine:
        {
            // Cosine interpolation: smooth S-curve from prev to next over the full cycle.
            const float t = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * p));
            return randomSinePrev + (randomSineNext - randomSinePrev) * t;
        }

        case LfoWaveform::RandomSquare:
            return randomSquareHeld;
    }

    return 0.0f;
}

float LfoProcessor::syncDivisionToHz (double bpm, SyncDivision div)
{
    // Division lengths as multiples of a quarter note.
    static constexpr double kDivBeats[] = {
        1.0 / 16.0,  // 1/64
        1.0 / 8.0,   // 1/32
        1.0 / 4.0,   // 1/16
        1.0 / 2.0,   // 1/8
        3.0 / 4.0,   // 1/8D
        1.0,         // 1/4
        1.5,         // 1/4D
        2.0,         // 1/2
        3.0,         // 1/2D
        4.0,         // 1/1
        6.0,         // 1/1D
        8.0          // 2/1
    };

    const int idx = static_cast<int> (div);
    const double beatsPerCycle = kDivBeats[idx];
    const double beatsPerSecond = bpm / 60.0;
    return static_cast<float> (beatsPerSecond / beatsPerCycle);
}
