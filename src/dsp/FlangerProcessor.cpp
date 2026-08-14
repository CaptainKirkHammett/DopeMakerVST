#include "FlangerProcessor.h"

void FlangerProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    chorus.prepare (spec);
    delayLfo.prepare  (spec.sampleRate);
    delayLfo.setWaveform (LfoWaveform::Sine);
    delayLfo.setDepth    (1.0f);   // full [-1..+1] output; mapped to [delayStart..delayEnd] below
}

void FlangerProcessor::reset()
{
    chorus.reset();
}

void FlangerProcessor::setParameters (float rateHz, float feedback,
                                       float delayStartMs, float delayEndMs, float mix)
{
    paramDelayStart = juce::jlimit (1.0f, 20.0f, delayStartMs);
    paramDelayEnd   = juce::jlimit (1.0f, 20.0f, delayEndMs);

    // Disable the chorus's own internal delay LFO — we drive centre delay from delayLfo.
    chorus.setRate        (0.0f);
    chorus.setDepth       (0.0f);
    chorus.setFeedback    (juce::jlimit (-0.99f, 0.99f, feedback));
    chorus.setMix         (mix);

    delayLfo.setRate (rateHz);
}

void FlangerProcessor::process (juce::dsp::ProcessContextReplacing<float>& context)
{
    const int numSamples = static_cast<int> (context.getOutputBlock().getNumSamples());

    // Advance delayLfo one block and compute current delay time.
    if (numSamples > 1)
        delayLfo.advance (numSamples - 1);
    const float lfoOut = delayLfo.getNextSample();   // [-1..+1]
    lfoOutput.store (lfoOut, std::memory_order_relaxed);

    // Map LFO to [delayStart..delayEnd]: lerp with unipolar signal.
    const float t = lfoOut * 0.5f + 0.5f;            // [0..1]
    const float delayMs = paramDelayStart + t * (paramDelayEnd - paramDelayStart);
    lfoDelayDelta.store (delayMs - paramDelayStart, std::memory_order_relaxed);
    chorus.setCentreDelay (delayMs);

    chorus.process (context);
}
