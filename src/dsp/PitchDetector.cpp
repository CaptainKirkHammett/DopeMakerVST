#include "PitchDetector.h"
#include <cmath>
#include <vector>

PitchDetector::Result PitchDetector::detect (const juce::AudioBuffer<float>& buffer,
                                              double sampleRate,
                                              float startNorm,
                                              float endNorm)
{
    const int totalSamples = buffer.getNumSamples();
    if (totalSamples == 0 || sampleRate <= 0.0)
        return {};

    // Clamp and apply start/end region.
    startNorm = juce::jlimit (0.0f, 1.0f, startNorm);
    endNorm   = juce::jlimit (0.0f, 1.0f, endNorm);
    if (endNorm <= startNorm)
        return {};

    const int regionStart = static_cast<int> (startNorm * totalSamples);
    const int regionEnd   = static_cast<int> (endNorm   * totalSamples);
    const int regionLen   = regionEnd - regionStart;
    if (regionLen < 256)
        return {};

    // Limit analysis to ~2 seconds within the region.
    const int maxSamples = juce::jmin (regionLen,
                                        static_cast<int> (sampleRate * 2.0));

    // Mono-mix the selected region into a temporary buffer.
    std::vector<float> mono (static_cast<size_t> (maxSamples), 0.0f);
    const int numChannels = buffer.getNumChannels();
    const float invCh = 1.0f / static_cast<float> (numChannels);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = buffer.getReadPointer (ch) + regionStart;
        for (int i = 0; i < maxSamples; ++i)
            mono[static_cast<size_t> (i)] += src[i] * invCh;
    }

    // Skip leading silence — find first sample exceeding a small threshold.
    constexpr float kSilenceThreshold = 0.01f;
    int onset = 0;
    for (int i = 0; i < maxSamples; ++i)
    {
        if (std::fabs (mono[static_cast<size_t> (i)]) > kSilenceThreshold)
        {
            onset = i;
            break;
        }
    }

    const int usableSamples = maxSamples - onset;
    if (usableSamples < 256)
        return {};

    // YIN with a fairly strict threshold for one-shot samples.
    constexpr float kYinThreshold = 0.15f;
    auto yin = yinEstimatePeriod (mono.data() + onset, usableSamples,
                                   kYinThreshold);

    if (yin.period <= 0.0f)
        return {};

    const float conf = juce::jlimit (0.0f, 1.0f, 1.0f - yin.cmndfMin / kYinThreshold);
    const float period = yin.period;

    const float freqHz = static_cast<float> (sampleRate) / period;

    // Sanity check — audible range only (20 Hz – 10 kHz).
    if (freqHz < 20.0f || freqHz > 10000.0f)
        return {};

    // Convert Hz to MIDI note number.
    const float midiFloat = 69.0f + 12.0f * std::log2 (freqHz / 440.0f);
    const int midiNote = juce::jlimit (0, 119, juce::roundToInt (midiFloat));

    return { freqHz, midiNote, conf };
}

PitchDetector::YinResult PitchDetector::yinEstimatePeriod (const float* data,
                                                           int numSamples,
                                                           float threshold)
{
    // YIN window size — half of the available samples, capped to a reasonable max.
    const int W = juce::jmin (numSamples / 2, 4096);
    if (W < 64)
        return { 0.0f, 1.0f };

    // Step 1 + 2: Difference function d(tau) and cumulative mean normalised
    // difference d'(tau).
    std::vector<float> cmndf (static_cast<size_t> (W), 0.0f);
    cmndf[0] = 1.0f;

    float runningSum = 0.0f;

    for (int tau = 1; tau < W; ++tau)
    {
        float diff = 0.0f;
        for (int i = 0; i < W; ++i)
        {
            const float delta = data[i] - data[i + tau];
            diff += delta * delta;
        }

        runningSum += diff;
        cmndf[static_cast<size_t> (tau)] = diff * static_cast<float> (tau) / runningSum;
    }

    // Step 3: Absolute threshold — find first tau where CMNDF drops below threshold.
    // Start from tau = 2 to avoid the trivial zero-lag peak.
    int bestTau = -1;
    float bestVal = 1.0f;

    for (int tau = 2; tau < W; ++tau)
    {
        if (cmndf[static_cast<size_t> (tau)] < threshold)
        {
            // Find the local minimum in this dip.
            while (tau + 1 < W
                   && cmndf[static_cast<size_t> (tau + 1)] < cmndf[static_cast<size_t> (tau)])
            {
                ++tau;
            }
            bestTau = tau;
            bestVal = cmndf[static_cast<size_t> (tau)];
            break;
        }
    }

    if (bestTau < 2)
        return { 0.0f, 1.0f };

    // Step 4: Parabolic interpolation around the minimum for sub-sample precision.
    if (bestTau > 0 && bestTau < W - 1)
    {
        const float s0 = cmndf[static_cast<size_t> (bestTau - 1)];
        const float s1 = cmndf[static_cast<size_t> (bestTau)];
        const float s2 = cmndf[static_cast<size_t> (bestTau + 1)];

        const float denom = 2.0f * (2.0f * s1 - s0 - s2);
        if (std::fabs (denom) > 1e-12f)
        {
            const float correction = (s0 - s2) / denom;
            return { static_cast<float> (bestTau) + correction, bestVal };
        }
    }

    return { static_cast<float> (bestTau), bestVal };
}
