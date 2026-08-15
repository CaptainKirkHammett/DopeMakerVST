#include "ReverbProcessor.h"
#include <algorithm>

void ReverbProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    lastSpec = spec;
    reverb.prepare (spec);

    const int maxSamples = static_cast<int> (spec.sampleRate * 4.0);
    for (auto& seg : segments)
    {
        seg.captureBuffer.setSize (static_cast<int> (spec.numChannels), maxSamples);
        seg.outputBuffer .setSize (static_cast<int> (spec.numChannels), maxSamples);
        seg.captureBuffer.clear();
        seg.outputBuffer .clear();
        seg.reverb.prepare (spec);
        seg.capturePos  = 0;
        seg.playbackPos = 0;
        seg.segmentLen  = 0;
        seg.outputReady = false;
    }
    activeCapture = 0;
}

void ReverbProcessor::reset()
{
    reverb.reset();
    for (auto& seg : segments)
    {
        seg.reverb.reset();
        seg.captureBuffer.clear();
        seg.outputBuffer .clear();
        seg.capturePos  = 0;
        seg.playbackPos = 0;
        seg.segmentLen  = 0;
        seg.outputReady = false;
    }
    activeCapture = 0;
}

void ReverbProcessor::setParameters (float roomSize, float damping, float width, float mix,
                                     bool reverse, float reverseLengthMs)
{
    currentRoomSize = roomSize;
    currentDamping  = damping;
    currentWidth    = width;
    currentMix      = mix;
    reverseEnabled  = reverse;

    const int maxLen = segments[0].captureBuffer.getNumSamples();
    reverseLengthSamps = juce::jlimit (
        1, maxLen,
        static_cast<int> (reverseLengthMs * 0.001f * static_cast<float> (lastSpec.sampleRate)));

    // Forward reverb params.
    juce::dsp::Reverb::Parameters params;
    params.roomSize   = roomSize;
    params.damping    = damping;
    params.width      = width;
    params.wetLevel   = mix;
    params.dryLevel   = 1.0f - mix;
    params.freezeMode = 0.0f;
    reverb.setParameters (params);

    // Reverse reverb instances: fully wet so we can mix manually.
    juce::dsp::Reverb::Parameters revParams;
    revParams.roomSize   = roomSize;
    revParams.damping    = damping;
    revParams.width      = width;
    revParams.wetLevel   = 1.0f;
    revParams.dryLevel   = 0.0f;
    revParams.freezeMode = 0.0f;
    for (auto& seg : segments)
        seg.reverb.setParameters (revParams);
}

void ReverbProcessor::processSegment (Segment& seg)
{
    const int len   = seg.capturePos;
    const int numCh = seg.captureBuffer.getNumChannels();

    if (len < 2)
        return;

    // Copy captured audio into output buffer, then reverse it.
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src = seg.captureBuffer.getReadPointer (ch);
        float*       dst = seg.outputBuffer.getWritePointer (ch);
        std::copy (src, src + len, dst);
        std::reverse (dst, dst + len);
    }

    // Process the reversed audio through this segment's reverb.
    seg.reverb.reset();
    juce::dsp::AudioBlock<float> block (seg.outputBuffer);
    block = block.getSubBlock (0, static_cast<size_t> (len));
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    seg.reverb.process (ctx);

    // Reverse the output back.
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* dst = seg.outputBuffer.getWritePointer (ch);
        std::reverse (dst, dst + len);
    }

    // Short fade-in/out to eliminate clicks at segment boundaries.
    const int fadeLen = juce::jmin (kFadeLen, len / 2);
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* dst = seg.outputBuffer.getWritePointer (ch);
        for (int i = 0; i < fadeLen; ++i)
        {
            const float gain = static_cast<float> (i) / static_cast<float> (fadeLen);
            dst[i]           *= gain;          // fade in
            dst[len - 1 - i] *= gain;          // fade out
        }
    }

    seg.segmentLen  = len;
    seg.playbackPos = 0;
    seg.outputReady = true;
}

void ReverbProcessor::process (juce::dsp::ProcessContextReplacing<float>& context)
{
    if (!reverseEnabled)
    {
        reverb.process (context);
        return;
    }

    auto& block      = context.getOutputBlock();
    const int numSamples = static_cast<int> (block.getNumSamples());
    const int numCh      = static_cast<int> (block.getNumChannels());

    auto& capSeg  = segments[activeCapture];
    auto& playSeg = segments[1 - activeCapture];

    for (int s = 0; s < numSamples; ++s)
    {
        // Capture incoming sample into the active capture segment.
        if (capSeg.capturePos < capSeg.captureBuffer.getNumSamples())
        {
            for (int ch = 0; ch < numCh; ++ch)
                capSeg.captureBuffer.setSample (ch, capSeg.capturePos, block.getSample (ch, s));
            ++capSeg.capturePos;
        }

        // When capture is full, process and swap roles.
        if (capSeg.capturePos >= reverseLengthSamps)
        {
            processSegment (capSeg);
            capSeg.capturePos = 0;

            // Swap: the just-processed segment becomes the playback segment,
            // the old playback segment becomes the new capture target.
            activeCapture = 1 - activeCapture;
        }

        // Mix output from the playback segment.
        const float dry = 1.0f - currentMix;
        const float wet = currentMix;

        float wetSample[2] = { 0.0f, 0.0f };

        // Primary playback segment.
        if (playSeg.outputReady && playSeg.playbackPos < playSeg.segmentLen)
        {
            for (int ch = 0; ch < numCh; ++ch)
                wetSample[ch] += playSeg.outputBuffer.getSample (ch, playSeg.playbackPos);
            ++playSeg.playbackPos;
        }

        // If the other segment (capSeg) also has output ready and is playing,
        // sum it in — this happens during the crossfade overlap after a swap.
        auto& otherSeg = segments[activeCapture];
        if (&otherSeg != &playSeg && otherSeg.outputReady && otherSeg.playbackPos < otherSeg.segmentLen)
        {
            for (int ch = 0; ch < numCh; ++ch)
                wetSample[ch] += otherSeg.outputBuffer.getSample (ch, otherSeg.playbackPos);
            ++otherSeg.playbackPos;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float drySample = block.getSample (ch, s);
            block.setSample (ch, s, drySample * dry + wetSample[ch] * wet);
        }
    }
}
