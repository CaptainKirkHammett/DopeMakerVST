#include "EnvelopeFollower.h"

void EnvelopeFollower::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = static_cast<float> (spec.sampleRate);
    envelope   = 0.0f;
    updateCoefficients();
}

void EnvelopeFollower::setAttackMs (float ms)
{
    if (ms == attackMs) return;
    attackMs = ms;
    updateCoefficients();
}

void EnvelopeFollower::setReleaseMs (float ms)
{
    if (ms == releaseMs) return;
    releaseMs = ms;
    updateCoefficients();
}

void EnvelopeFollower::updateCoefficients()
{
    attackCoef  = std::exp (-1.0f / (attackMs  * 0.001f * sampleRate));
    releaseCoef = std::exp (-1.0f / (releaseMs * 0.001f * sampleRate));
}

float EnvelopeFollower::process (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int s = 0; s < numSamples; ++s)
    {
        // Take the peak across all channels at this sample position.
        float peak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax (peak, std::abs (buffer.getSample (ch, s)));

        const float coef = (peak > envelope) ? attackCoef : releaseCoef;
        envelope = coef * envelope + (1.0f - coef) * peak;
    }

    return envelope;
}
