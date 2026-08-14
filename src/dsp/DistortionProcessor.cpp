#include "DistortionProcessor.h"
#include <cmath>

DistortionProcessor::DistortionProcessor()
{
    rebuildShaper();
}

void DistortionProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    lastSpec = spec;

    // Crossover filters — Butterworth 2nd-order, Q = 1/sqrt(2).
    constexpr float kLowMidHz  = 350.0f;
    constexpr float kMidHighHz = 3500.0f;
    constexpr float kQ         = 0.7071f;

    *lowLP.state  = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (spec.sampleRate, kLowMidHz,  kQ);
    *midHP.state  = *juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, kLowMidHz,  kQ);
    *midLP.state  = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (spec.sampleRate, kMidHighHz, kQ);
    *highHP.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (spec.sampleRate, kMidHighHz, kQ);

    lowLP.prepare  (spec);
    midHP.prepare  (spec);
    midLP.prepare  (spec);
    highHP.prepare (spec);

    const int ch      = static_cast<int> (spec.numChannels);
    const int maxSamp = static_cast<int> (spec.maximumBlockSize);
    lowBuf .setSize (ch, maxSamp);
    midBuf .setSize (ch, maxSamp);
    highBuf.setSize (ch, maxSamp);

    shaper.prepare (spec);
    outputGain.prepare (spec);

    driveSmoothed    .reset (spec.sampleRate, 0.02);
    lowDriveSmoothed .reset (spec.sampleRate, 0.02);
    midDriveSmoothed .reset (spec.sampleRate, 0.02);
    highDriveSmoothed.reset (spec.sampleRate, 0.02);

    driveSmoothed.setCurrentAndTargetValue (drive);
    lowDriveSmoothed .setCurrentAndTargetValue (0.0f);
    midDriveSmoothed .setCurrentAndTargetValue (0.0f);
    highDriveSmoothed.setCurrentAndTargetValue (0.0f);

    ringModSampleRate = spec.sampleRate;
}

void DistortionProcessor::reset()
{
    lowLP.reset();
    midHP.reset();
    midLP.reset();
    highHP.reset();
    shaper.reset();
    outputGain.reset();
    ringModPhase = 0.0f;
}

void DistortionProcessor::setMode (DistMode mode)
{
    if (mode == currentMode)
        return;
    currentMode = mode;
    rebuildShaper();
    shaper.prepare (lastSpec);
}

void DistortionProcessor::setDrive (float zeroToOne)
{
    drive = juce::jlimit (0.0f, 1.0f, zeroToOne);
    driveSmoothed.setTargetValue (drive);
}

void DistortionProcessor::setOutputGain (float dB)
{
    outputGain.setGainDecibels (juce::jlimit (-24.0f, 0.0f, dB));
}

void DistortionProcessor::setBitDepth (int bits)
{
    bitDepth = juce::jlimit (2, 16, bits);
    if (currentMode == DistMode::BitCrush)
        rebuildShaper();
}

void DistortionProcessor::setBandDrives (float low, float mid, float high)
{
    lowDriveSmoothed .setTargetValue (juce::jlimit (0.0f, 1.0f, low));
    midDriveSmoothed .setTargetValue (juce::jlimit (0.0f, 1.0f, mid));
    highDriveSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, high));
}

void DistortionProcessor::applyBandDrive (juce::AudioBuffer<float>& buf, int numSamples,
                                           juce::SmoothedValue<float>& driveVal)
{
    const int numCh = buf.getNumChannels();
    for (int s = 0; s < numSamples; ++s)
    {
        // drive=0 → gain 1.0 (tanh(x)≈x, transparent); drive=1 → gain 11.0 (heavy saturation).
        const float gain = driveVal.getNextValue() * 10.0f + 1.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* p = buf.getWritePointer (ch);
            p[s] = std::tanh (p[s] * gain);
        }
    }
}

void DistortionProcessor::rebuildShaper()
{
    const int   bd     = bitDepth;
    const float levels = std::pow (2.0f, static_cast<float> (bd - 1));

    switch (currentMode)
    {
        case DistMode::SoftSaturation:
            shaper.functionToUse = [] (float x) { return std::tanh (x); };
            break;

        case DistMode::HardClip:
            shaper.functionToUse = [] (float x) {
                return juce::jlimit (-1.0f, 1.0f, x);
            };
            break;

        case DistMode::Foldback:
            shaper.functionToUse = [] (float x) {
                return std::abs (std::fmod (x + 1.0f, 4.0f) - 2.0f) - 1.0f;
            };
            break;

        case DistMode::BitCrush:
            shaper.functionToUse = [levels] (float x) {
                return std::round (x * levels) / levels;
            };
            break;

        case DistMode::Waveshaper:
            // Cubic Padé approximant — smooth saturation distinct from tanh.
            shaper.functionToUse = [] (float x) {
                return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
            };
            break;

        case DistMode::HardSaturation:
            // Algebraic saturation x/sqrt(1+x²) — reaches ±1 asymptotically,
            // crunchy odd-harmonic character, more aggressive knee than tanh.
            shaper.functionToUse = [] (float x) {
                return x / std::sqrt (1.0f + x * x);
            };
            break;

        case DistMode::SoftClip:
            // Cubic knee: linear inside ±1.5, smoothly reaches ±1 at the boundary,
            // hard-clips anything beyond ±1.5.
            shaper.functionToUse = [] (float x) {
                const float cx = juce::jlimit (-1.5f, 1.5f, x);
                return cx * (1.0f - (cx * cx) / 6.75f);
            };
            break;

        case DistMode::RingModulation:
            // No-op shaper — ring mod is handled in process() via oscillator state.
            shaper.functionToUse = [] (float x) { return x; };
            break;
    }
}

void DistortionProcessor::process (juce::dsp::ProcessContextReplacing<float>& context)
{
    auto& block = context.getOutputBlock();
    const int numSamples  = static_cast<int> (block.getNumSamples());
    const int numChannels = static_cast<int> (block.getNumChannels());

    // ── Multiband drive ───────────────────────────────────────────────────────
    // Copy input into 3 scratch buffers, filter each into its frequency band,
    // apply per-band tanh saturation, then sum back into the main block.

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = block.getChannelPointer (ch);
        juce::FloatVectorOperations::copy (lowBuf .getWritePointer (ch), src, numSamples);
        juce::FloatVectorOperations::copy (midBuf .getWritePointer (ch), src, numSamples);
        juce::FloatVectorOperations::copy (highBuf.getWritePointer (ch), src, numSamples);
    }

    auto lowBlock  = juce::dsp::AudioBlock<float> (lowBuf) .getSubBlock (0, (size_t) numSamples);
    auto midBlock  = juce::dsp::AudioBlock<float> (midBuf) .getSubBlock (0, (size_t) numSamples);
    auto highBlock = juce::dsp::AudioBlock<float> (highBuf).getSubBlock (0, (size_t) numSamples);

    lowLP .process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
    midHP .process (juce::dsp::ProcessContextReplacing<float> (midBlock));
    midLP .process (juce::dsp::ProcessContextReplacing<float> (midBlock));
    highHP.process (juce::dsp::ProcessContextReplacing<float> (highBlock));

    applyBandDrive (lowBuf,  numSamples, lowDriveSmoothed);
    applyBandDrive (midBuf,  numSamples, midDriveSmoothed);
    applyBandDrive (highBuf, numSamples, highDriveSmoothed);

    // Sum the three driven bands back into the main block.
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float*       dst  = block.getChannelPointer (ch);
        const float* low  = lowBuf .getReadPointer (ch);
        const float* mid  = midBuf .getReadPointer (ch);
        const float* high = highBuf.getReadPointer (ch);
        for (int s = 0; s < numSamples; ++s)
            dst[s] = low[s] + mid[s] + high[s];
    }

    // ── Main drive + shaper + output gain ─────────────────────────────────────
    if (currentMode == DistMode::RingModulation)
    {
        // Drive knob maps to oscillator frequency: 50 Hz (drive=0) → 5000 Hz (drive=1).
        const float freqHz  = 50.0f * std::pow (100.0f, drive);
        const float phaseInc = juce::MathConstants<float>::twoPi
                               * freqHz / static_cast<float> (ringModSampleRate);

        // Advance drive smoother unconditionally so it stays in sync on mode changes.
        for (int s = 0; s < numSamples; ++s) driveSmoothed.getNextValue();

        for (int s = 0; s < numSamples; ++s)
        {
            const float osc = std::sin (ringModPhase);
            ringModPhase += phaseInc;
            if (ringModPhase >= juce::MathConstants<float>::twoPi)
                ringModPhase -= juce::MathConstants<float>::twoPi;

            for (int ch = 0; ch < numChannels; ++ch)
                block.getChannelPointer (ch)[s] *= osc;
        }
    }
    else
    {
        for (int s = 0; s < numSamples; ++s)
        {
            const float d = driveSmoothed.getNextValue() * 10.0f + 1.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                block.getChannelPointer (ch)[s] *= d;
        }

        shaper.process (context);
    }

    outputGain.process (context);
}
