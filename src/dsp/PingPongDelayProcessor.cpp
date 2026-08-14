#include "PingPongDelayProcessor.h"
#include <cmath>

PingPongDelayProcessor::PingPongDelayProcessor() = default;

void PingPongDelayProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // Allocate delay lines for up to 2 seconds.
    const int maxDelaySamples = static_cast<int> (spec.sampleRate * 2.0) + 1;
    delayLineL.setMaximumDelayInSamples (maxDelaySamples);
    delayLineR.setMaximumDelayInSamples (maxDelaySamples);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    delayLineL.prepare (monoSpec);
    delayLineR.prepare (monoSpec);

    fbIirL.reset();
    fbIirR.reset();

    fbLfo.prepare  (spec.sampleRate);
    mixLfo.prepare (spec.sampleRate);

    feedbackSmoothed.reset   (spec.sampleRate, 0.02);
    mixSmoothed.reset        (spec.sampleRate, 0.02);
    delayTimeLSmoothed.reset (spec.sampleRate, 0.05);
    delayTimeRSmoothed.reset (spec.sampleRate, 0.05);

    feedbackSmoothed.setCurrentAndTargetValue   (0.4f);
    mixSmoothed.setCurrentAndTargetValue        (0.35f);

    const float samplesL = delayTimeLMs * 0.001f * static_cast<float> (spec.sampleRate);
    const float samplesR = delayTimeRMs * 0.001f * static_cast<float> (spec.sampleRate);
    delayTimeLSmoothed.setCurrentAndTargetValue (samplesL);
    delayTimeRSmoothed.setCurrentAndTargetValue (samplesR);

    // Duck envelope: fast attack so the delay ducks quickly when input arrives,
    // slow release so it fades back in gracefully when input stops.
    duckAttackCoeff  = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.015));
    duckReleaseCoeff = 1.0f - std::exp (-1.0f / static_cast<float> (sampleRate * 0.250));
    duckEnvLevel     = 0.0f;

    updateFbFilterCoefficients();
}

void PingPongDelayProcessor::reset()
{
    delayLineL.reset();
    delayLineR.reset();
    fbIirL.reset();
    fbIirR.reset();
    duckEnvLevel = 0.0f;
}

void PingPongDelayProcessor::setDuckAmount (float zeroToOne)
{
    duckAmount = juce::jlimit (0.0f, 1.0f, zeroToOne);
    duckLfoDepth.store (duckAmount * 0.5f, std::memory_order_relaxed);
    if (duckAmount <= 0.0f)
    {
        duckEnvLevel = 0.0f;
        duckLfoOutput.store (0.0f, std::memory_order_relaxed);
    }
}

void PingPongDelayProcessor::killTail()
{
    killRequested.store (true, std::memory_order_relaxed);
}

void PingPongDelayProcessor::setDelayTimes (float leftMs, float rightMs)
{
    delayTimeLMs = leftMs;
    delayTimeRMs = rightMs;
    const float samplesL = leftMs  * 0.001f * static_cast<float> (sampleRate);
    const float samplesR = rightMs * 0.001f * static_cast<float> (sampleRate);
    delayTimeLSmoothed.setTargetValue (samplesL);
    delayTimeRSmoothed.setTargetValue (samplesR);
}

void PingPongDelayProcessor::setTempoSync (bool /*enabled*/, double bpm,
                                            SyncDivision leftDiv, SyncDivision rightDiv)
{
    const float hzL = LfoProcessor::syncDivisionToHz (bpm, leftDiv);
    const float hzR = LfoProcessor::syncDivisionToHz (bpm, rightDiv);
    setDelayTimes (1000.0f / hzL, 1000.0f / hzR);
}

void PingPongDelayProcessor::setFeedback (float zeroToNinetyNine)
{
    const float clamped = juce::jlimit (0.0f, 0.99f, zeroToNinetyNine * 0.01f);
    feedbackSmoothed.setTargetValue (clamped);
}

void PingPongDelayProcessor::setMix (float zeroToOne)
{
    // Store user-set level; actual smoothed target is computed in process() after LFO offset.
    baseMix = juce::jlimit (0.0f, 1.0f, zeroToOne);
}

void PingPongDelayProcessor::syncMixLfoToTransport (double ppqPosition, float rateHz, double bpm)
{
    if (ppqPosition < 0.0 || bpm <= 0.0 || rateHz <= 0.0f)
        return;

    // phase = ppqPosition / beatsPerCycle
    // beatsPerCycle = (bpm/60) / rateHz  →  phase = ppqPosition * rateHz * 60 / bpm
    const double cyclesElapsed = ppqPosition * static_cast<double> (rateHz) * 60.0 / bpm;
    mixLfo.forcePhase (cyclesElapsed);
}

void PingPongDelayProcessor::setMixLfoParams (LfoWaveform waveform, float rateHz, float depth)
{
    mixLfoDepthCache = depth;
    mixLfo.setWaveform (waveform);
    mixLfo.setRate     (rateHz);
    mixLfo.setDepth    (1.0f);  // raw -1..+1 output; depth applied manually so UI can read it
}

void PingPongDelayProcessor::setFbFilterParams (FilterMode mode, float cutoffHz, float resonance)
{
    currentFbMode      = mode;
    baseFbCutoff       = cutoffHz;
    currentFbResonance = resonance;
    updateFbFilterCoefficients();
}

void PingPongDelayProcessor::setFbLfoParams (LfoWaveform waveform, float rateHz, float depth)
{
    fbLfoDepthCache = depth;
    fbLfo.setWaveform (waveform);
    fbLfo.setRate     (rateHz);
    fbLfo.setDepth    (1.0f);  // raw -1..+1 output; depth applied manually so UI can read it
}

void PingPongDelayProcessor::updateFbFilterCoefficients()
{
    // LFO modulates the cutoff logarithmically around the base value.
    // Called once per block; lfoOffset is pre-computed before this call.
    const float fc = juce::jlimit (20.0f, 20000.0f, baseFbCutoff);
    const float q  = juce::jlimit (0.1f,  10.0f,    currentFbResonance);

    juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
    switch (currentFbMode)
    {
        case FilterMode::LowPass:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, fc, q); break;
        case FilterMode::HighPass:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, fc, q); break;
        case FilterMode::BandPass:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, fc, q); break;
    }

    *fbIirL.coefficients = *coeffs;
    *fbIirR.coefficients = *coeffs;
}

void PingPongDelayProcessor::applyKillTailIfRequested()
{
    if (killRequested.exchange (false, std::memory_order_relaxed))
    {
        delayLineL.reset();
        delayLineR.reset();
        fbIirL.reset();
        fbIirR.reset();
    }
}

void PingPongDelayProcessor::process (juce::dsp::ProcessContextReplacing<float>& context)
{
    applyKillTailIfRequested();

    auto& block = context.getOutputBlock();
    const int numSamples = static_cast<int> (block.getNumSamples());
    const int numCh      = static_cast<int> (block.getNumChannels());

    float* chL = block.getChannelPointer (0);
    float* chR = (numCh > 1) ? block.getChannelPointer (1) : chL;

    // Advance LFO once per block and modulate the feedback filter cutoff.
    // LFO output (-depth..+depth) is treated as a semitone-equivalent log shift.
    if (numSamples > 1)
        fbLfo.advance (numSamples - 1);
    const float fbRawOut = fbLfo.getNextSample();           // -1..+1 (depth always 1)
    fbLfoOutput.store (fbRawOut, std::memory_order_relaxed);
    const float lfoOut = fbRawOut * fbLfoDepthCache;        // depth-scaled for audio use

    // Map LFO output to cutoff: multiply base by 2^(lfoOut * 2) to swing ±2 octaves at full depth.
    const float modulatedCutoff = juce::jlimit (
        20.0f, 20000.0f,
        baseFbCutoff * std::exp2 (lfoOut * 2.0f));
    currentFbMode; // mode unchanged by LFO
    const float q = juce::jlimit (0.1f, 10.0f, currentFbResonance);

    juce::dsp::IIR::Coefficients<float>::Ptr modCoeffs;
    switch (currentFbMode)
    {
        case FilterMode::LowPass:
            modCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, modulatedCutoff, q); break;
        case FilterMode::HighPass:
            modCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, modulatedCutoff, q); break;
        case FilterMode::BandPass:
            modCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, modulatedCutoff, q); break;
    }
    *fbIirL.coefficients = *modCoeffs;
    *fbIirR.coefficients = *modCoeffs;

    // Duck envelope — measure block peak from the input signal, then advance a one-pole
    // AR envelope (closed-form N-step update avoids a per-sample inner loop).
    // duckAmount scales the maximum reduction so 0.0 = no duck, 1.0 = full duck.
    float duckReduction = 0.0f;
    if (duckAmount > 0.0f)
    {
        float blockPeak = 0.0f;
        for (int s = 0; s < numSamples; ++s)
        {
            blockPeak = juce::jmax (blockPeak, std::abs (chL[s]));
            blockPeak = juce::jmax (blockPeak, std::abs (chR[s]));
        }

        const float coeff = (blockPeak > duckEnvLevel) ? duckAttackCoeff : duckReleaseCoeff;
        // After N identical steps the envelope converges to: target - (target - env0) * (1-k)^N
        const float decayN = std::pow (1.0f - coeff, static_cast<float> (numSamples));
        duckEnvLevel = blockPeak - (blockPeak - duckEnvLevel) * decayN;
        duckEnvLevel = juce::jlimit (0.0f, 1.0f, duckEnvLevel);

        duckReduction = duckEnvLevel * 0.5f * duckAmount;
        duckLfoOutput.store (-duckEnvLevel, std::memory_order_relaxed);
    }

    // Advance mix LFO once per block; modulate dry/wet around the user-set baseMix.
    // Duck subtracts up to 0.5 when the input signal is present.
    if (numSamples > 1)
        mixLfo.advance (numSamples - 1);
    const float mixRawOut    = mixLfo.getNextSample();              // -1..+1 (depth always 1)
    mixLfoOutput.store (mixRawOut, std::memory_order_relaxed);
    const float mixLfoOut    = mixRawOut * mixLfoDepthCache;        // depth-scaled for audio use
    const float modulatedMix = juce::jlimit (0.0f, 1.0f, baseMix + mixLfoOut - duckReduction);
    mixSmoothed.setTargetValue (modulatedMix);

    // Compensate for the Q-induced resonance peak of the IIR feedback filter.
    // A 2nd-order biquad with Q > 1 amplifies the signal by ~Q at the resonant
    // frequency; without compensation that multiplies into the feedback gain and
    // causes self-oscillation at moderate feedback settings.
    const float filterGainComp = 1.0f / juce::jmax (1.0f, currentFbResonance);

    // Per-sample ping-pong delay with cross-channel feedback routing.
    for (int s = 0; s < numSamples; ++s)
    {
        delayTimeLSmoothed.skip (1);
        delayTimeRSmoothed.skip (1);

        const float dtL = juce::jmax (1.0f, delayTimeLSmoothed.getCurrentValue());
        const float dtR = juce::jmax (1.0f, delayTimeRSmoothed.getCurrentValue());
        delayLineL.setDelay (dtL);
        delayLineR.setDelay (dtR);

        const float fbGain  = feedbackSmoothed.getNextValue();
        const float mixGain = mixSmoothed.getNextValue();

        // Read delayed outputs.
        const float delayedL = delayLineL.popSample (0);
        const float delayedR = delayLineR.popSample (0);

        // Apply feedback filter to the delayed signal.
        // filterGainComp is applied only to the re-injected feedback signal, NOT to the
        // output. This prevents Q-peak self-oscillation without making the wet signal
        // quieter or reducing feedback gain at non-resonant frequencies.
        const float filteredL = fbIirL.processSample (delayedL);
        const float filteredR = fbIirR.processSample (delayedR);

        // Ping-pong cross-routing: filtered L feeds R delay input and vice versa.
        // Q-peak compensation is applied here so loop gain at resonance ≈ fbGain.
        const float fbL = filteredL * fbGain * filterGainComp;
        const float fbR = filteredR * fbGain * filterGainComp;
        delayLineL.pushSample (0, chL[s] + fbR);
        delayLineR.pushSample (0, chR[s] + fbL);

        // Output the filtered delayed signal so every echo passes through the filter,
        // not just the re-injected feedback. This makes the filter fully audible.
        chL[s] = chL[s] * (1.0f - mixGain) + filteredL * mixGain;
        chR[s] = chR[s] * (1.0f - mixGain) + filteredR * mixGain;
    }
}
