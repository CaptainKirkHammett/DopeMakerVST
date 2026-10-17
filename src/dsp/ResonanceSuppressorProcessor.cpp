#include "ResonanceSuppressorProcessor.h"
#include <cmath>
#include <algorithm>

// ── Shared ───────────────────────────────────────────────────────────────────

void ResonanceSuppressorProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // IIR bands
    initBandFrequencies();

    for (auto& b : bands)
    {
        b.bpL.reset();
        b.bpR.reset();
        b.energy      = 0.0f;
        b.energyAccum = 0.0f;
        b.reduction   = 0.0f;
    }

    updateCoefficients();

    reductionAttackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.002f));
    reductionReleaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.030f));

    // FFT overlap-add
    for (auto& ch : olaChannels)
    {
        ch.inputRing.fill (0.0f);
        ch.outputRing.fill (0.0f);
        ch.writePos   = 0;
        ch.hopCounter = 0;
    }

    prevBinGain.fill (1.0f);
    initWindow();
}

void ResonanceSuppressorProcessor::reset()
{
    for (auto& b : bands)
    {
        b.bpL.reset();
        b.bpR.reset();
        b.energy      = 0.0f;
        b.energyAccum = 0.0f;
        b.reduction   = 0.0f;
    }

    for (auto& ch : olaChannels)
    {
        ch.inputRing.fill (0.0f);
        ch.outputRing.fill (0.0f);
        ch.writePos   = 0;
        ch.hopCounter = 0;
    }

    prevBinGain.fill (1.0f);
}

void ResonanceSuppressorProcessor::process (juce::AudioBuffer<float>& buffer, float amount, bool fftMode)
{
    if (amount <= 0.0f)
        return;

    if (fftMode)
        processFFT (buffer, amount);
    else
        processIIR (buffer, amount);
}

// ── IIR band-based mode ──────────────────────────────────────────────────────

void ResonanceSuppressorProcessor::initBandFrequencies()
{
    const double logMin = std::log (static_cast<double> (kMinFreq));
    const double logMax = std::log (static_cast<double> (kMaxFreq));

    for (int i = 0; i < kNumBands; ++i)
    {
        const double t = static_cast<double> (i) / static_cast<double> (kNumBands - 1);
        bands[i].centreFreq = static_cast<float> (std::exp (logMin + t * (logMax - logMin)));
    }
}

void ResonanceSuppressorProcessor::updateCoefficients()
{
    constexpr float Q = 2.5f;

    for (auto& b : bands)
    {
        const float fc = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.49), b.centreFreq);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, fc, Q);
        *b.bpL.coefficients = *coeffs;
        *b.bpR.coefficients = *coeffs;
    }
}

void ResonanceSuppressorProcessor::processIIR (juce::AudioBuffer<float>& buffer, float amount)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float avgEnergy = 0.0f;
    for (const auto& b : bands)
        avgEnergy += b.energy;
    avgEnergy /= static_cast<float> (kNumBands);

    std::array<float, kNumBands> targetReduction {};

    for (int i = 0; i < kNumBands; ++i)
    {
        float neighborEnergy = 0.0f;
        int   neighborCount  = 0;

        if (i > 0)              { neighborEnergy += bands[i - 1].energy; ++neighborCount; }
        if (i < kNumBands - 1)  { neighborEnergy += bands[i + 1].energy; ++neighborCount; }

        const float ref = (neighborCount > 0)
            ? neighborEnergy / static_cast<float> (neighborCount)
            : avgEnergy;

        const float ratio = (ref > 1e-12f) ? (bands[i].energy / ref) : 0.0f;

        const float threshold = 1.02f + (1.0f - amount) * 0.78f;
        const float excess = juce::jmax (0.0f, ratio - threshold);

        targetReduction[i] = juce::jmin (1.0f, excess * amount * 3.0f);

        bands[i].energyAccum = 0.0f;
    }

    auto* dataL = buffer.getWritePointer (0);
    auto* dataR = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int s = 0; s < numSamples; ++s)
    {
        float subL = 0.0f;
        float subR = 0.0f;

        for (int i = 0; i < kNumBands; ++i)
        {
            auto& b = bands[i];

            const float bpOutL = b.bpL.processSample (dataL[s]);
            const float bpOutR = dataR ? b.bpR.processSample (dataR[s]) : 0.0f;

            b.energyAccum += bpOutL * bpOutL + bpOutR * bpOutR;

            const float target = targetReduction[i];
            const float coeff  = (target > b.reduction) ? reductionAttackCoeff : reductionReleaseCoeff;
            b.reduction += (target - b.reduction) * coeff;

            subL += bpOutL * b.reduction;
            if (dataR) subR += bpOutR * b.reduction;
        }

        dataL[s] -= subL;
        if (dataR) dataR[s] -= subR;
    }

    const float invN = 1.0f / juce::jmax (1.0f, static_cast<float> (numSamples));
    for (auto& b : bands)
        b.energy = b.energyAccum * invN;
}

// ── FFT spectral mode ────────────────────────────────────────────────────────

void ResonanceSuppressorProcessor::initWindow()
{
    for (int i = 0; i < kFFTSize; ++i)
        window[i] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi
                                              * static_cast<float> (i)
                                              / static_cast<float> (kFFTSize)));
}

void ResonanceSuppressorProcessor::processFFTFrame (float* fftData, float amount)
{
    std::array<float, kNumBins> mag {};
    for (int i = 0; i < kNumBins; ++i)
    {
        const float re = fftData[i * 2];
        const float im = fftData[i * 2 + 1];
        mag[i] = std::sqrt (re * re + im * im);
    }

    const int halfWin = juce::jlimit (2, 12, static_cast<int> (3.0f + (1.0f - amount) * 9.0f));

    std::array<float, kNumBins> binGain {};

    for (int i = 0; i < kNumBins; ++i)
    {
        float neighborSum = 0.0f;
        int   neighborCnt = 0;

        for (int j = juce::jmax (0, i - halfWin); j <= juce::jmin (kNumBins - 1, i + halfWin); ++j)
        {
            if (j == i) continue;
            neighborSum += mag[j];
            ++neighborCnt;
        }

        const float ref = (neighborCnt > 0) ? neighborSum / static_cast<float> (neighborCnt) : mag[i];

        const float ratio = (ref > 1e-12f) ? (mag[i] / ref) : 0.0f;

        const float threshold = 1.05f + (1.0f - amount) * 0.95f;
        const float excess    = juce::jmax (0.0f, ratio - threshold);

        const float reduction = juce::jmin (1.0f, excess * amount * 2.5f);
        const float rawGain   = 1.0f - reduction;

        const float smoothCoeff = 0.7f;
        binGain[i] = rawGain * (1.0f - smoothCoeff) + prevBinGain[i] * smoothCoeff;
        prevBinGain[i] = binGain[i];
    }

    for (int i = 0; i < kNumBins; ++i)
    {
        fftData[i * 2]     *= binGain[i];
        fftData[i * 2 + 1] *= binGain[i];
    }
}

void ResonanceSuppressorProcessor::processFFT (juce::AudioBuffer<float>& buffer, float amount)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto& ola  = olaChannels[ch];
        auto* data = buffer.getWritePointer (ch);

        for (int s = 0; s < numSamples; ++s)
        {
            // Write input into circular buffer.
            ola.inputRing[ola.writePos] = data[s];

            // Read output from the circular accumulator (one full window behind).
            data[s] = ola.outputRing[ola.writePos];
            ola.outputRing[ola.writePos] = 0.0f;

            ola.writePos = (ola.writePos + 1) % kFFTSize;
            ++ola.hopCounter;

            if (ola.hopCounter >= kHopSize)
            {
                ola.hopCounter = 0;

                // Gather the last kFFTSize samples from the circular input buffer
                // and apply the analysis window (Hann).
                std::array<float, kFFTSize * 2> fftBuf {};
                for (int i = 0; i < kFFTSize; ++i)
                {
                    const int idx = (ola.writePos + i) % kFFTSize;
                    fftBuf[i] = ola.inputRing[idx] * window[i];
                }

                fft.performRealOnlyForwardTransform (fftBuf.data(), true);

                processFFTFrame (fftBuf.data(), amount);

                fft.performRealOnlyInverseTransform (fftBuf.data());

                // Overlap-add into the circular output accumulator.
                // No synthesis window — single Hann + 50% overlap satisfies COLA.
                for (int i = 0; i < kFFTSize; ++i)
                {
                    const int idx = (ola.writePos + i) % kFFTSize;
                    ola.outputRing[idx] += fftBuf[i];
                }
            }
        }
    }
}
