#include "FilterProcessor.h"

void FilterProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    lastSpec = spec;
    filter.prepare (spec);
    needsUpdate = true;

    // 3ms ramp — fast enough to track modulation, slow enough to prevent clicks.
    smoothedCutoff.reset (spec.sampleRate, 0.003);
    smoothedCutoff.setCurrentAndTargetValue (currentCutoff);
    smoothingActive = false;

    combDelayBuf.setSize (static_cast<int> (spec.numChannels), kCombBufSize);
    combDelayBuf.clear();
    combWritePos = 0;
}

void FilterProcessor::reset()
{
    filter.reset();
    combDelayBuf.clear();
    combWritePos = 0;
}

void FilterProcessor::setParameters (FilterMode mode, float cutoffHz, float resonance)
{
    const bool modeChanged = (mode != currentMode);
    const bool qChanged    = (std::abs (resonance - currentQ) > 0.001f);

    currentMode = mode;
    currentQ    = resonance;

    // Always update the smoother target; it will ramp to the new cutoff.
    smoothedCutoff.setTargetValue (cutoffHz);
    smoothingActive = smoothedCutoff.isSmoothing();

    if (modeChanged || qChanged)
    {
        currentCutoff = cutoffHz;
        needsUpdate   = true;
    }
}

void FilterProcessor::updateCoefficients()
{
    if (!needsUpdate)
        return;

    needsUpdate = false;

    const double sr = lastSpec.sampleRate;
    const float  fc = juce::jlimit (20.0f, static_cast<float> (sr * 0.49), currentCutoff);
    const float  q  = juce::jlimit (0.1f, 10.0f, currentQ);

    // Comb modes use a delay line — no IIR coefficients needed.
    if (currentMode == FilterMode::CombPlus || currentMode == FilterMode::CombMinus)
    {
        combDelaySamples = juce::jlimit (1, kCombBufSize - 1,
                                         static_cast<int> (sr / static_cast<double> (fc)));
        // Map resonance [0.1, 10.0] → feedback coeff [0.0, 0.9].
        combFeedback = juce::jlimit (0.0f, 0.9f, (q - 0.1f) / 9.9f * 0.9f);
        return;
    }

    juce::dsp::IIR::Coefficients<float>::Ptr coeffs;

    switch (currentMode)
    {
        case FilterMode::LowPass:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, fc, q);
            break;
        case FilterMode::HighPass:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, fc, q);
            break;
        case FilterMode::BandPass:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, fc, q);
            // makeBandPass peak gain is 1/Q, so scale the numerator by Q
            // to maintain unity gain at the center frequency.
            {
                auto* raw = coeffs->getRawCoefficients();
                for (int i = 0; i < 3; ++i)
                    raw[i] *= q;
            }
            break;
        default:
            return;
    }

    *filter.state = *coeffs;
}

void FilterProcessor::process (juce::dsp::ProcessContextReplacing<float>& context)
{
    if (currentMode == FilterMode::CombPlus || currentMode == FilterMode::CombMinus)
    {
        // Comb modes: smoothing applied inside processComb via delay length.
        currentCutoff = smoothedCutoff.skip (static_cast<int> (context.getOutputBlock().getNumSamples()));
        needsUpdate = true;
        updateCoefficients();
        processComb (context);
        return;
    }

    // IIR modes: if the cutoff is smoothing, process in small sub-blocks
    // with coefficient updates between them to ramp smoothly.
    if (smoothingActive)
    {
        auto& block = context.getOutputBlock();
        const int totalSamples = static_cast<int> (block.getNumSamples());

        // Sub-block size: small enough for smooth cutoff ramp, large enough
        // to keep CPU reasonable.
        constexpr int kChunkSize = 8;
        int pos = 0;

        while (pos < totalSamples)
        {
            const int chunkLen = juce::jmin (kChunkSize, totalSamples - pos);
            const float fc = smoothedCutoff.skip (chunkLen);

            if (std::abs (fc - currentCutoff) > 0.5f)
            {
                currentCutoff = fc;
                needsUpdate   = true;
                updateCoefficients();
            }

            auto subBlock = block.getSubBlock (static_cast<size_t> (pos),
                                               static_cast<size_t> (chunkLen));
            juce::dsp::ProcessContextReplacing<float> subCtx (subBlock);
            filter.process (subCtx);

            pos += chunkLen;
        }

        smoothingActive = smoothedCutoff.isSmoothing();
        return;
    }

    // Not smoothing — batch process as before.
    currentCutoff = smoothedCutoff.getCurrentValue();
    needsUpdate = true;
    updateCoefficients();
    filter.process (context);
}

void FilterProcessor::processComb (juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();
    const int numSamples  = static_cast<int> (block.getNumSamples());
    const int numChannels = juce::jmin (static_cast<int> (block.getNumChannels()),
                                        combDelayBuf.getNumChannels());
    const float sign = (currentMode == FilterMode::CombPlus) ? 1.0f : -1.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        const int readPos = (combWritePos - combDelaySamples + kCombBufSize) & (kCombBufSize - 1);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* out   = block.getChannelPointer (ch);
            float* delay = combDelayBuf.getWritePointer (ch);

            const float x = out[s];
            const float y = x + sign * combFeedback * delay[readPos];
            delay[combWritePos] = y;
            out[s] = y;
        }

        combWritePos = (combWritePos + 1) & (kCombBufSize - 1);
    }
}
