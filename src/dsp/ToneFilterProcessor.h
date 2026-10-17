#pragma once
#include <juce_dsp/juce_dsp.h>

// Shared LP / HP tone filter used by the Highcut and Lowcut panels.
// Amount (0–1) selects 1–8 cascaded 2nd-order Butterworth stages (12–96 dB/oct).
// Q is fixed at 1/√2 (≈ 0.707) so no frequency ever gets boosted;
// only the rolloff slope increases with each added stage.
class ToneFilterProcessor
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& ctx);
    void reset   ();

    // isLowpass=true → Highcut panel; isLowpass=false → Lowcut panel.
    void setParameters (bool isLowpass, float freqHz, float amount);

private:
    static constexpr int kMaxStages = 8;

    using StereoIIR = juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>>;

    StereoIIR stages[kMaxStages];
    int       activeStages { 1 };
    double    sampleRate   { 44100.0 };
    bool      currentLP    { true };
    float     currentFreq  { 1000.0f };
};
