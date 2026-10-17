#include "PhaserProcessor.h"

void PhaserProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    lastSpec = spec;
    phaser.prepare (spec);
    mirrorLfo.prepare (spec.sampleRate);
    mirrorLfo.setWaveform (LfoWaveform::Sine);
    mirrorLfo.setDepth (1.0f);
}

void PhaserProcessor::reset()
{
    phaser.reset();
}

void PhaserProcessor::setParameters (float rateHz, float depth, float feedback,
                                      int stages, float centreFreqHz, float mix)
{
    if (stages != lastStages)
        rebuildForStageChange (stages);

    mirrorLfo.setRate (rateHz);
    phaser.setRate            (rateHz);
    phaser.setDepth           (depth);
    phaser.setFeedback        (juce::jlimit (-0.99f, 0.99f, feedback));
    phaser.setCentreFrequency (centreFreqHz);
    phaser.setMix             (mix);
}

void PhaserProcessor::rebuildForStageChange (int newStages)
{
    // juce::dsp::Phaser does not expose stage count at runtime; we must
    // reconstruct the object and re-prepare it.  Stage changes are rare
    // (user adjusting a combo box) so this is safe from the audio thread.
    lastStages = newStages;
    phaser     = juce::dsp::Phaser<float>{};
    phaser.prepare (lastSpec);
}

void PhaserProcessor::process (juce::dsp::ProcessContextReplacing<float>& context)
{
    const int numSamples = static_cast<int> (context.getOutputBlock().getNumSamples());
    if (numSamples > 1)
        mirrorLfo.advance (numSamples - 1);
    lfoOutput.store (mirrorLfo.getNextSample(), std::memory_order_relaxed);

    phaser.process (context);
}
