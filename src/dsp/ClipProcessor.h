#pragma once

#include <juce_dsp/juce_dsp.h>

enum class ClipMode { Hard = 0, Soft = 1 };

class ClipProcessor
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& ctx);
    void reset();

    void setMode        (ClipMode mode);
    void setAmount      (float zeroToOne);   // 0 = clean, 1 = heavy
    void setMakeupGain  (float dB);          // 0–24 dB post-clip gain

private:
    juce::SmoothedValue<float> amountSmoothed;
    juce::SmoothedValue<float> makeupSmoothed;
    ClipMode currentMode { ClipMode::Hard };
};
