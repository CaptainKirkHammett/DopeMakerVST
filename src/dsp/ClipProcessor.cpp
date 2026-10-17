#include "ClipProcessor.h"
#include <cmath>

void ClipProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    amountSmoothed.reset (spec.sampleRate, 0.02);
    amountSmoothed.setCurrentAndTargetValue (0.5f);
    makeupSmoothed.reset (spec.sampleRate, 0.02);
    makeupSmoothed.setCurrentAndTargetValue (1.0f);
}

void ClipProcessor::reset()
{
    amountSmoothed.setCurrentAndTargetValue (amountSmoothed.getTargetValue());
    makeupSmoothed.setCurrentAndTargetValue (makeupSmoothed.getTargetValue());
}

void ClipProcessor::setMakeupGain (float dB)
{
    makeupSmoothed.setTargetValue (juce::Decibels::decibelsToGain (dB));
}

void ClipProcessor::setMode (ClipMode mode)
{
    currentMode = mode;
}

void ClipProcessor::setAmount (float zeroToOne)
{
    amountSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, zeroToOne));
}

void ClipProcessor::process (juce::dsp::ProcessContextReplacing<float>& ctx)
{
    auto& block      = ctx.getOutputBlock();
    const int numCh  = static_cast<int> (block.getNumChannels());
    const int numSam = static_cast<int> (block.getNumSamples());

    for (int s = 0; s < numSam; ++s)
    {
        const float amount = amountSmoothed.getNextValue();
        const float makeup = makeupSmoothed.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* p = block.getChannelPointer (ch);
            const float in = p[s];

            float out;
            if (currentMode == ClipMode::Hard)
            {
                // Threshold lowers from 1.0 → 0.05 as amount increases.
                const float threshold = 1.0f - 0.95f * amount;
                out = juce::jlimit (-threshold, threshold, in);
            }
            else
            {
                // Blend dry→tanh so amount=0 is fully clean.
                // At amount=1: drive=20, peaks compressed toward ±0.05.
                const float drive = 1.0f + amount * 19.0f;
                const float wet   = std::tanh (in * drive) / drive;
                out = in + amount * (wet - in);
            }
            p[s] = out * makeup;
        }
    }
}
