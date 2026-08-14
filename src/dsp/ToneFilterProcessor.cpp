#include "ToneFilterProcessor.h"

// Butterworth Q for a 2nd-order section: 1/√2 ≈ 0.7071.
// At this value the passband is maximally flat and no frequency is ever boosted.
static constexpr float kButterworthQ = 0.7071067811865476f;

void ToneFilterProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (auto& s : stages)
        s.prepare (spec);
    setParameters (currentLP, currentFreq, 0.0f);
}

void ToneFilterProcessor::reset()
{
    for (auto& s : stages)
        s.reset();
}

void ToneFilterProcessor::setParameters (bool isLowpass, float freqHz, float amount)
{
    currentLP   = isLowpass;
    currentFreq = juce::jlimit (20.0f, 20000.0f, freqHz);

    // Map amount linearly to 1–8 cascaded Butterworth stages (12–96 dB/oct).
    // Each stage adds 12 dB/oct; thresholds are evenly spaced at every 1/8 of range.
    const float amt = juce::jlimit (0.0f, 1.0f, amount);
    activeStages = juce::jlimit (1, kMaxStages,
                                 static_cast<int> (amt * kMaxStages) + 1);

    // Build coefficients once — all stages share the same fc and Q.
    auto coeffs = isLowpass
        ? juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, currentFreq, kButterworthQ)
        : juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, currentFreq, kButterworthQ);

    for (auto& s : stages)
        *s.state = *coeffs;
}

void ToneFilterProcessor::process (juce::dsp::ProcessContextReplacing<float>& ctx)
{
    // Chain only the active stages; each call reads the in-place buffer
    // left by the previous stage, producing a cascaded response.
    for (int i = 0; i < activeStages; ++i)
        stages[i].process (ctx);
}
