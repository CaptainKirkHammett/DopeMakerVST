#include "StereoProcessor.h"

void StereoProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    // 7 ms delay — comfortably inside the Haas fusion zone (no audible echo),
    // long enough that autocorrelation with broadband audio is near zero.
    delaySamples = juce::roundToInt (spec.sampleRate * 0.007);
    jassert (delaySamples > 0 && delaySamples < kMaxDelay);

    widthSmoothed.reset (spec.sampleRate, 0.02);  // 20 ms ramp
    widthSmoothed.setCurrentAndTargetValue (0.0f);

    reset();
}

void StereoProcessor::setWidth (float width)
{
    widthSmoothed.setTargetValue (width);
}

void StereoProcessor::process (juce::dsp::AudioBlock<float>& block)
{
    float* L = block.getChannelPointer (0);
    float* R = block.getChannelPointer (1);
    const int numSamples = static_cast<int> (block.getNumSamples());

    for (int s = 0; s < numSamples; ++s)
    {
        const float w = widthSmoothed.getNextValue();   // -1..+1

        const float mid  = (L[s] + R[s]) * 0.5f;
        const float side = (L[s] - R[s]) * 0.5f;

        // Natural side scale: 0 at w=-1 (mono), 1 at w=0, 2 at w=+1.
        const float sideFactor = w + 1.0f;
        const float outSide    = side * sideFactor;

        // Haas-style artificial widening.  Read the delayed mid before writing
        // the current mid so there is always exactly delaySamples of lag.
        const int   readPtr    = (delayWritePtr - delaySamples + kMaxDelay) % kMaxDelay;
        const float delayedMid = midDelay[readPtr];
        midDelay[delayWritePtr] = mid;
        delayWritePtr = (delayWritePtr + 1) % kMaxDelay;

        // Artificial side only active when widening (w > 0).
        const float wPos    = juce::jmax (0.0f, w);
        const float artSide = delayedMid * wPos * 0.5f;

        L[s] = mid + outSide + artSide;
        R[s] = mid - outSide - artSide;
    }
}

void StereoProcessor::reset()
{
    midDelay.fill (0.0f);
    delayWritePtr = 0;
    widthSmoothed.setCurrentAndTargetValue (widthSmoothed.getTargetValue());
}
