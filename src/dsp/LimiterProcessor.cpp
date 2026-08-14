#include "LimiterProcessor.h"

void LimiterProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    lastBlockSize = static_cast<int> (spec.maximumBlockSize);

    limiter.prepare (spec);
    limiter.setRelease (80.0f);  // 80 ms release — tight but not pumpy

    outputGainSmoothed.reset (spec.sampleRate, 0.02);  // 20 ms ramp
    outputGainSmoothed.setCurrentAndTargetValue (1.0f);
}

void LimiterProcessor::setAmount (float amount)
{
    // Threshold: 0 dB at amount=0, -24 dB at amount=1.
    const float thresholdDb = amount * -24.0f;
    currentThresholdDb = thresholdDb;
    limiter.setThreshold (thresholdDb);

    // Output compensation: reduce by up to -7.2 dB at max amount so that the
    // limited signal does not end up louder than the dry signal.
    const float compensationDb = amount * -7.2f;
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (compensationDb));
}

void LimiterProcessor::process (juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();

    // Per-sample EMA of the input amplitude (both channels combined).
    // Coefficient 0.9997 ≈ 33 ms time constant at 44100 Hz — smooth like a
    // level follower without the lookahead timing issues of block-peak comparison.
    const size_t numCh = block.getNumChannels();
    for (size_t s = 0; s < block.getNumSamples(); ++s)
    {
        float maxSample = 0.0f;
        for (size_t ch = 0; ch < numCh; ++ch)
            maxSample = std::max (maxSample, std::abs (block.getChannelPointer (ch)[s]));
        avgInput = 0.9997f * avgInput + 0.0003f * maxSample;
    }

    // GR in dB: how far the smoothed input level sits above the current threshold.
    const float avgInputDb = (avgInput > 1e-7f) ? 20.0f * std::log10 (avgInput) : -140.0f;
    gainReductionDb.store (std::max (0.0f, avgInputDb - currentThresholdDb),
                           std::memory_order_relaxed);

    limiter.process (context);

    // Apply smoothed output compensation sample-by-sample.
    const int numSamples  = static_cast<int> (block.getNumSamples());
    const int numChannels = static_cast<int> (block.getNumChannels());

    for (int s = 0; s < numSamples; ++s)
    {
        const float g = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            block.getChannelPointer (static_cast<size_t> (ch))[s] *= g;
    }
}

void LimiterProcessor::reset()
{
    limiter.reset();
    avgInput = 0.0f;
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
    outputGainSmoothed.setCurrentAndTargetValue (outputGainSmoothed.getTargetValue());
}
