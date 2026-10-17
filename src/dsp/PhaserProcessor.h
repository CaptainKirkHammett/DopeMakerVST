#pragma once

#include <juce_dsp/juce_dsp.h>
#include "LfoProcessor.h"

class PhaserProcessor
{
public:
    PhaserProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& context);
    void reset();

    void setParameters (float rateHz, float depth, float feedback,
                        int stages, float centreFreqHz, float mix);

    // Raw LFO output written on the audio thread; read by the UI mod indicator.
    std::atomic<float> lfoOutput    { 0.0f };
    std::atomic<float> lfoFullDepth { 1.0f };  // constant — used as depth pointer for LfoModIndicator

private:
    void rebuildForStageChange (int newStages);

    juce::dsp::Phaser<float>   phaser;
    juce::dsp::ProcessSpec     lastSpec { 44100.0, 512, 2 };
    int                        lastStages { 4 };
    LfoProcessor               mirrorLfo;  // mirrors phaser's internal LFO for UI display
};
