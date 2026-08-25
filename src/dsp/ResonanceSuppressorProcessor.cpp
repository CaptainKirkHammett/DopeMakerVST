#include "ResonanceSuppressorProcessor.h"

void ResonanceSuppressorProcessor::initBandFrequencies()
{
    // Logarithmically space 12 bands from 300 Hz to 10 kHz.
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
    // Q=1.5 gives broader bands with good overlap — catches resonances
    // that fall between band centres without leaving spectral gaps.
    constexpr float Q = 1.5f;

    for (auto& b : bands)
    {
        const float fc = juce::jlimit (20.0f, static_cast<float> (sampleRate * 0.49), b.centreFreq);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, fc, Q);
        *b.bpL.coefficients = *coeffs;
        *b.bpR.coefficients = *coeffs;
    }
}

void ResonanceSuppressorProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

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

    // Attack: ~2 ms — fast enough to catch transient resonances.
    reductionAttackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.002f));
    // Release: ~30 ms — smooth enough to avoid pumping.
    reductionReleaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float> (sampleRate) * 0.030f));
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
}

void ResonanceSuppressorProcessor::process (juce::AudioBuffer<float>& buffer, float amount)
{
    if (amount <= 0.0f)
        return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // --- Compute reduction gains from previous block's energy measurements ---
    // Compare each band to its immediate neighbors; bands that protrude are suppressed.
    float avgEnergy = 0.0f;
    for (const auto& b : bands)
        avgEnergy += b.energy;
    avgEnergy /= static_cast<float> (kNumBands);

    std::array<float, kNumBands> targetReduction {};

    for (int i = 0; i < kNumBands; ++i)
    {
        // Compare to immediate neighbors only — don't blend with global average
        // so a broadband resonance can't hide itself by lifting the global mean.
        float neighborEnergy = 0.0f;
        int   neighborCount  = 0;

        if (i > 0)              { neighborEnergy += bands[i - 1].energy; ++neighborCount; }
        if (i < kNumBands - 1)  { neighborEnergy += bands[i + 1].energy; ++neighborCount; }

        // Edge bands: use the one available neighbor, or fall back to global average.
        const float ref = (neighborCount > 0)
            ? neighborEnergy / static_cast<float> (neighborCount)
            : avgEnergy;

        // How much this band exceeds its neighbors (in linear energy ratio).
        const float ratio = (ref > 1e-12f) ? (bands[i].energy / ref) : 0.0f;

        // Threshold: at amount=1.0 → 1.02 (nearly any imbalance triggers),
        // at amount→0 → 1.8 (only obvious peaks).
        const float threshold = 1.02f + (1.0f - amount) * 0.78f;
        const float excess = juce::jmax (0.0f, ratio - threshold);

        // Reduction: very aggressive — small excess drives full subtraction quickly.
        targetReduction[i] = juce::jmin (1.0f, excess * amount * 3.0f);

        // Reset energy accumulator for this block.
        bands[i].energyAccum = 0.0f;
    }

    // --- Process samples: bandpass, measure energy, subtract weighted bands ---
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

            // Accumulate energy for next block's analysis.
            b.energyAccum += bpOutL * bpOutL + bpOutR * bpOutR;

            // Smooth the reduction gain toward its target (fast attack, slower release).
            const float target = targetReduction[i];
            const float coeff  = (target > b.reduction) ? reductionAttackCoeff : reductionReleaseCoeff;
            b.reduction += (target - b.reduction) * coeff;

            subL += bpOutL * b.reduction;
            if (dataR) subR += bpOutR * b.reduction;
        }

        dataL[s] -= subL;
        if (dataR) dataR[s] -= subR;
    }

    // Store energy for next block (normalised by sample count).
    const float invN = 1.0f / juce::jmax (1.0f, static_cast<float> (numSamples));
    for (auto& b : bands)
        b.energy = b.energyAccum * invN;
}
