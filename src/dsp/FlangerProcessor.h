#pragma once

#include <juce_dsp/juce_dsp.h>
#include "LfoProcessor.h"

// Flanger based on juce::dsp::Chorus at short centre-delay times.
// Feedback sweeps between fbStart and fbEnd each LFO cycle (rate-synced).
class FlangerProcessor
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& context);
    void reset();

    // delayStart/delayEnd define the LFO delay-time sweep range in ms [1, 20].
    void setParameters (float rateHz, float feedback,
                        float delayStartMs, float delayEndMs, float mix);

    // Raw LFO output written on the audio thread; read by the UI mod indicator.
    std::atomic<float> lfoOutput     { 0.0f };
    std::atomic<float> lfoFullDepth  { 1.0f };  // constant — used as depth pointer for LfoModIndicator
    // Delta from delayStart to the current sweep position (ms); used by LFO St indicator.
    std::atomic<float> lfoDelayDelta { 0.0f };

private:
    juce::dsp::Chorus<float> chorus;
    LfoProcessor             delayLfo;   // sweeps centre-delay between delayStart and delayEnd
    float                    paramDelayStart { 1.0f };
    float                    paramDelayEnd   { 5.0f };
};
