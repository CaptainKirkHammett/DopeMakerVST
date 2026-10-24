#include "SidechainProcessor.h"

void SidechainProcessor::prepare (double sr)
{
    sampleRate   = sr;
    // 2 ms one-pole smoother to eliminate clicks on abrupt envelope changes.
    smoothCoeff  = 1.0f - std::exp (-1.0f / (float (sr) * 0.002f));
    reset();
}

void SidechainProcessor::reset()
{
    phase        = 0.0;
    gainSmoothed = 1.0f;
}

//==============================================================================
float SidechainProcessor::computeEnvelope (float preDelay01, float rise01, float curve, float fall01) const
{
    const float totalSpan = juce::jmax (1e-5f, 1.0f - preDelay01);
    const float riseSpan  = juce::jmax (1e-5f, juce::jlimit (0.01f, 1.0f, rise01) * totalSpan);
    const float riseEnd   = preDelay01 + riseSpan;

    // Fall span: inverted so fall=0 → maximum fall (gradual), fall=1 → instant drop.
    const float maxFallSpan = juce::jmax (0.0f, 1.0f - riseEnd);
    const float fallSpan    = (1.0f - fall01) * maxFallSpan;
    const float fallStart   = 1.0f - fallSpan;

    // Map curve [0..1] to exponent: curve=0 → slow rise, curve=0.5 → linear, curve=1 → fast rise.
    const float exponent = std::pow (2.0f, 3.0f - 6.0f * curve);

    // Pre-delay region — fully muted (volume = 0).
    if ((float) phase <= preDelay01)
        return 0.0f;

    // Rise region — gain climbs from 0 to 1.
    if ((float) phase < riseEnd)
    {
        const float pos = juce::jlimit (0.0f, 1.0f, (float) (phase - preDelay01) / riseSpan);
        return std::pow (pos, exponent);
    }

    // Fall region — linear descent from 1 to 0 (independent of curve shape).
    if (fallSpan > 1e-5f && (float) phase >= fallStart)
    {
        const float pos = juce::jlimit (0.0f, 1.0f, (float) (phase - fallStart) / fallSpan);
        return 1.0f - pos;
    }

    // Hold region — fully open (volume = 1).
    return 1.0f;
}

void SidechainProcessor::setDrawnEnvelope (const float* table, int tableSize)
{
    drawnTable     = table;
    drawnTableSize = tableSize;
}

float SidechainProcessor::lookupDrawnEnvelope() const
{
    if (drawnTable == nullptr || drawnTableSize <= 0)
        return 1.0f;

    const float idx = static_cast<float> (phase) * static_cast<float> (drawnTableSize);
    const int   i0  = juce::jlimit (0, drawnTableSize - 1, static_cast<int> (idx));
    const int   i1  = juce::jlimit (0, drawnTableSize - 1, i0 + 1);
    const float frac = idx - static_cast<float> (i0);
    return drawnTable[i0] + frac * (drawnTable[i1] - drawnTable[i0]);
}

void SidechainProcessor::process (juce::AudioBuffer<float>& buffer,
                                   double       bpm,
                                   SyncDivision division,
                                   float        preDelay01,
                                   float        rise01,
                                   float        curve,
                                   float        fall01,
                                   float        mix,
                                   double       ppqPosition,
                                   bool         useDrawnEnvelope)
{
    if (mix <= 0.0f)
        return;

    // Division lengths in quarter notes — mirrors LfoProcessor::kDivBeats.
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

    const double divBeats = kDivBeats[static_cast<int> (division)];

    const float  hz            = LfoProcessor::syncDivisionToHz (bpm, division);
    const double periodSamples = (hz > 0.0f) ? (sampleRate / hz) : sampleRate;
    const double phaseInc      = 1.0 / periodSamples;

    // Lock phase to DAW transport when PPQ is available.
    const bool   hasTransport  = ppqPosition >= 0.0;
    const double ppqPerSample  = (bpm > 0.0) ? (bpm / (60.0 * sampleRate)) : 0.0;

    if (hasTransport)
        phase = std::fmod (ppqPosition, divBeats) / divBeats;

    const int n  = buffer.getNumSamples();
    const int nC = buffer.getNumChannels();

    for (int s = 0; s < n; ++s)
    {
        const float env     = useDrawnEnvelope ? lookupDrawnEnvelope()
                                               : computeEnvelope (preDelay01, rise01, curve, fall01);
        // Blend: gain=1 at mix=0, gain=env at mix=1.  Clamp to avoid negative gain.
        const float rawGain = juce::jmax (0.0f, 1.0f + mix * (env - 1.0f));
        // One-pole LP smoother kills clicks from abrupt gain steps.
        gainSmoothed += (rawGain - gainSmoothed) * smoothCoeff;

        for (int ch = 0; ch < nC; ++ch)
            buffer.getWritePointer (ch)[s] *= gainSmoothed;

        if (hasTransport)
        {
            // Advance phase using PPQ increment for sample-accurate grid lock.
            phase += ppqPerSample / divBeats;
        }
        else
        {
            phase += phaseInc;
        }

        if (phase >= 1.0)
            phase -= 1.0;
    }
}
