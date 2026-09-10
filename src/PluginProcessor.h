#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "ParameterIDs.h"
#include "engine/SamplerEngine.h"
#include "dsp/FilterProcessor.h"
#include "dsp/DistortionProcessor.h"
#include "dsp/FlangerProcessor.h"
#include "dsp/PhaserProcessor.h"
#include "dsp/ReverbProcessor.h"
#include "dsp/PingPongDelayProcessor.h"
#include "dsp/LfoProcessor.h"
#include "dsp/GateController.h"
#include "dsp/LimiterProcessor.h"
#include "dsp/StereoProcessor.h"
#include "dsp/SidechainProcessor.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/ArpeggiatorProcessor.h"
#include "dsp/ClipProcessor.h"
#include "dsp/ToneFilterProcessor.h"
#include "dsp/GranulizerProcessor.h"
#include "dsp/ResonanceSuppressorProcessor.h"
#include "dsp/StutterProcessor.h"
#include "PresetManager.h"
#include "SpectrumSource.h"
#include "StereoFieldSource.h"

class PluginProcessor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "Dope Maker"; }
    bool acceptsMidi() const override        { return true; }
    bool producesMidi() const override       { return false; }
    bool isMidiEffect() const override       { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    //==============================================================================
    int getNumPrograms() override            { return 1; }
    int getCurrentProgram() override         { return 0; }
    void setCurrentProgram (int) override    {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Loads audio from file into the engine. Returns true on success.
    bool loadSampleFile (const juce::File& file);

    SamplerEngine* getEngine() { return engine.get(); }

    const juce::File& getLastLoadedFile() const { return lastLoadedFile; }

    // Set by setStateInformation when the saved sample path no longer exists.
    // Read on the message thread by PluginEditor to show a warning in the waveform display.
    bool         sampleMissing     { false };
    juce::String missingSampleName;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager { apvts };
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 5 };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Public so PluginEditor can access without a separate accessor.
    GranulizerProcessor    granulizerProc;
    StutterProcessor       stutterProc;
    PingPongDelayProcessor delayProc;
    FlangerProcessor       flangerProc;
    ReverbProcessor        reverbProc;
    PhaserProcessor        phaserProc;
    GateController         gateController;
    SidechainProcessor     sidechainProc;
    EnvelopeFollower       audioEnvFollower;

    ArpeggiatorProcessor arpProc;

    // Filter LFO raw output — written on the audio thread, read by the UI mod indicator.
    std::atomic<float> filterLfoOutput { 0.0f };

    // Stutter Len LFO — written on the audio thread, read by the UI mod indicator.
    std::atomic<float> stutterLenLfoOutput { 0.0f };

    // VU-meter level feeds — written on the audio thread, read on the UI thread.
    // Stores the block peak (linear amplitude) for each channel.
    std::atomic<float> vuPreFxL  { 0.0f };
    std::atomic<float> vuPreFxR  { 0.0f };
    std::atomic<float> vuPostFxL { 0.0f };
    std::atomic<float> vuPostFxR { 0.0f };

    // Gate step level and audio envelope follower level — written on the audio thread, read on the UI thread.
    std::atomic<float> audioEnvLevelForUI { 0.0f };

    // Post-stereo-processor L/R feed for the goniometer visualiser.
    StereoFieldSource stereoFieldSource;

    // Post-lowcut spectrum data (after both highcut and lowcut) — written on the audio thread, read on the UI thread.
    SpectrumSource lowcutSpectrumSource;

    // Clip FX visualization feeds — written on the audio thread, read on the UI thread.
    // clipPrePeak*  : block peak BEFORE the clip processor (linear amplitude).
    // clipThreshold : the effective clip ceiling for the current block (linear amplitude).
    // clipModeState : ClipMode index (0=Hard, 1=Soft), or -1 when clip is disabled.
    std::atomic<float> clipPrePeakL  { 0.0f };
    std::atomic<float> clipPrePeakR  { 0.0f };
    std::atomic<float> clipThreshold { 1.0f };
    std::atomic<int>   clipModeState { -1   };

    // Limiter gain reduction — written on the audio thread, read by the UI.
    // Positive dB: how much the brick-wall stage clamped the last block. 0 = no limiting.
    std::atomic<float> limiterGainReductionDb { 0.0f };

    // Peak-normalize gain factor (1 / peak) — set in loadSampleFile, read by processBlock.
    std::atomic<float> normalizeGain { 1.0f };

    // Request a DSP flush from the UI thread — the actual flush happens at the
    // start of the next processBlock() so it is thread-safe.
    void requestDspFlush() { dspFlushRequested.store (true, std::memory_order_release); }

private:
    // Clears all DSP buffers/state — must only be called on the audio thread.
    void flushAllDspState();

    // Set by the UI thread, checked/cleared at the top of processBlock().
    std::atomic<bool> dspFlushRequested { false };

    std::unique_ptr<SamplerEngine> engine;
    FilterProcessor      filterProc;
    DistortionProcessor  distProc;
    ToneFilterProcessor  highcutProc;
    ToneFilterProcessor  lowcutProc;
    ClipProcessor        clipProc;
    LfoProcessor        lfo;
    LfoProcessor        velLfo;
    LfoProcessor        filterLfo;
    LfoProcessor        stutterLenLfo;     // modulates stutter Len parameter

    // Pre-FX gain — smoothed to prevent zipper noise on automation.
    juce::SmoothedValue<float> gainSmoothed;

    // Pre-gain (before engine) and post-gain (after entire FX chain) — smoothed.
    juce::SmoothedValue<float> preGainSmoothed;
    juce::SmoothedValue<float> postGainSmoothed;

    // Peak-normalize gain — smoothed to avoid clicks when toggling.
    juce::SmoothedValue<float> normalizeSmoothed { 1.0f };

    // Step volume gate — smoothed to prevent clicks from step transitions.
    juce::SmoothedValue<float> gateVolumeSmoothed { 1.0f };

    // FX enable crossfade smoothers — 5ms ramp to eliminate clicks on toggle.
    juce::SmoothedValue<float> filterEnableSmoothed  { 0.0f };
    juce::SmoothedValue<float> distEnableSmoothed    { 0.0f };
    juce::SmoothedValue<float> flangerEnableSmoothed { 0.0f };
    juce::SmoothedValue<float> phaserEnableSmoothed  { 0.0f };
    juce::SmoothedValue<float> reverbEnableSmoothed  { 0.0f };
    juce::SmoothedValue<float> delayEnableSmoothed   { 0.0f };
    juce::SmoothedValue<float> highcutEnableSmoothed { 0.0f };
    juce::SmoothedValue<float> lowcutEnableSmoothed  { 0.0f };
    juce::SmoothedValue<float> clipEnableSmoothed    { 0.0f };
    juce::SmoothedValue<float> suppressorEnableSmoothed { 0.0f };

    // Shared scratch buffer for dry/wet crossfade during FX enable transitions.
    juce::AudioBuffer<float> fxDryBuffer;

    // Resonance suppressor — last in chain, after clip.
    ResonanceSuppressorProcessor suppressorProc;

    // Pre-FX stereo width — first in chain so limiter and gain act on shaped stereo.
    StereoProcessor stereoProc;

    // Pre-FX limiter — sits after stereo width, before gain.
    LimiterProcessor limiterProc;

    // Cached raw parameter pointers for gate step amplitudes — avoids per-block string lookup.
    std::array<std::atomic<float>*, GateController::kMaxSteps> gateStepPtrs {};

    juce::File lastLoadedFile;

    void setupLfoNoteOnCallback();
    void updateLfoAndPushOffset    (int numSamples);
    void updateVelLfoAndPushOffset (int numSamples);

    // APVTS::Listener — enforces sampleStartPos < sampleEndPos.
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // Reads the current BPM from the play head; returns 120.0 if unavailable.
    double getCurrentBpm() const;

    // Re-entrancy guard so parameterChanged doesn't call itself recursively.
    bool enforcingMarkerConstraint { false };

    // Kill-tail edge detection — tracks the previous block's value so the
    // trigger fires exactly once per 0→1 transition (UI click or DAW automation).
    bool prevKillTailState { false };

    // Transport state tracking — used to reset the gate on play start and on scrub,
    // and to lock tempo-synced LFOs to the DAW timeline.
    bool   wasPlaying        { false };
    double lastPpqPosition   { -1.0 };  // -1 = unknown
    double currentPpqPosition { -1.0 }; // start-of-block PPQ; -1 = unknown / not playing
    bool   currentIsPlaying   { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
