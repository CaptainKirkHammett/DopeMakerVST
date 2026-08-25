#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>

#include "AdsrEnvelope.h"
#include "../dsp/LfoProcessor.h"

// Shared sample data passed from SamplerEngine to each SamplerVoice.
// Reference-counted: voices hold a shared_ptr so the buffer stays alive
// for the duration of the note even if a new sample is loaded mid-play.
struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sourceSampleRate { 44100.0 };
    float  peakNormalizeGain { 1.0f };   // 1.0 / peak amplitude — applied when normalize is on
};

enum class LoopMode { Forward, PingPong };

struct VoiceParams
{
    float startPos        { 0.0f };   // 0–1 normalised (marker position — loop boundary)
    float endPos          { 1.0f };   // 0–1 normalised (marker position — loop boundary)
    float initialReadPos  { -1.0f };  // 0–1 normalised; where playback actually begins
                                      // (may differ from startPos/endPos due to LFO).
                                      // Negative means "use startPos / endPos as before".
    bool  perChannelZC    { false };  // true = independent ZC per channel; false = shared earliest ZC
    bool  reversed        { false };  // play end→start
    bool  randomDir       { false };  // true = direction randomised per note in engine
    bool  looping         { false };
    LoopMode loopMode     { LoopMode::Forward };
    int   rootNote        { 48 };     // MIDI note for 1:1 pitch
    int   semitoneOffset  { 0 };      // -24..+24 coarse transposition
    int   pitchBendRange  { 2 };      // semitones
    // portamentoMs > 0 also enables legato (envelope/sample not restarted in mono mode)
    float portamentoMs    { 0.0f };   // pitch glide time in ms (0 = instant / legato off)
    // Volume ADSR
    float volAttackMs     { 10.0f };
    float volDecayMs      { 100.0f };
    float volSustain      { 1.0f };
    float volReleaseMs    { 200.0f };
    // Pitch ADSR
    float pitchAttackMs   { 0.0f };
    float pitchDecayMs    { 500.0f };
    float pitchSustain    { 0.0f };
    float pitchReleaseMs  { 100.0f };
    float pitchEnvDepth   { 12.0f };  // semitones
    // Volume Release LFO (read by engine to configure shared LFO)
    int   volRelLfoWaveform     { 0 };    // LfoWaveform index
    float volRelLfoRate         { 1.0f }; // Hz (free-run)
    float volRelLfoDepth        { 0.0f }; // 0–1
    float volRelLfoEffectiveRateHz { 1.0f }; // resolved from rate or tempo-sync in PluginProcessor
    // Pitch LFO (read by engine to configure shared LFO)
    int   pitchLfoWaveform        { 0 };    // LfoWaveform index
    float pitchLfoEffectiveRateHz { 1.0f }; // resolved in PluginProcessor
    float pitchLfoDepth           { 0.0f }; // semitones (0–64)
};

// Shared LFO state passed from SamplerEngine to each SamplerVoice during
// rendering.  The engine owns the LFO instances and pre-renders per-sample
// pitch LFO values into a buffer; voices index into it by sample offset.
struct SharedLfoState
{
    const float* pitchLfoBuffer;       // per-sample pitch LFO values [0..blockSize-1]
    float        pitchLfoDepthSemi;    // depth in semitones [0..64]
    float        volRelLfoSample;      // single per-block vol release LFO value [-1..+1]
    float        volRelLfoDepth;       // raw depth [0..1]
    float        volRelBaseReleaseMs;  // base release time from APVTS
};

class SamplerVoice
{
public:
    SamplerVoice() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);

    void startNote (int midiNote, float velocity,
                    std::shared_ptr<const SampleData> sample,
                    const VoiceParams& params);

    // allowTailOff = true triggers release; false hard-stops.
    void stopNote (bool allowTailOff);

    // Blends to silence over ~64 samples, then re-triggers with newNote/newVelocity.
    // Used for voice stealing to avoid clicks.
    void stealAndRestart (int newMidiNote, float newVelocity,
                          std::shared_ptr<const SampleData> newSample,
                          const VoiceParams& newParams);

    // Legato retrigger: changes pitch target without restarting the envelope or sample.
    // Only called in mono mode when legato is enabled and the voice is already active.
    void retriggerPitch (int newMidiNote, float newVelocity, const VoiceParams& newParams);

    void setPitchBend (float semitones);

    // Hard-kill: immediately deactivate and clear all state.  Used on transport
    // jumps where the audio is already discontinuous and a click is acceptable.
    void forceOff();

    // Renders this voice into outputBuffer, mixing (adding) into existing content.
    // lfoState provides sample-accurate shared LFO values from the engine.
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples,
                          const SharedLfoState& lfoState);

    bool isActive() const;
    bool isNoteHeld() const { return noteHeld; }
    int  getCurrentMidiNote() const { return midiNote; }

    // Returns the current read position as a 0..1 fraction of the sample length.
    // Only valid when isActive() is true; called from the audio thread.
    float getNormalizedReadPos() const;

    uint64_t noteOnTimestamp { 0 };

private:
    void renderSamples (juce::AudioBuffer<float>& outputBuffer,
                        int startSample, int numSamples,
                        const SharedLfoState& lfoState);

    float computePlaybackRatio (float pitchEnvSample, float pitchLfoSemi) const;
    void  advanceReadPosition (float ratio);
    float readSampleLinear (float position, int channel) const;
    void  handleLoopBoundary();

    // State
    std::shared_ptr<const SampleData> sampleData;
    VoiceParams        params;
    int                midiNote  { 60 };
    float              velocity  { 1.0f };
    double             readPos   { 0.0 };  // sub-sample position in frames

    // Per-channel read offsets (frames added to readPos when reading each channel).
    // Computed once in startNote from independent per-channel zero-crossing detection.
    static constexpr int kMaxChannels = 2;
    int chReadOffset[kMaxChannels] { 0, 0 };

    // Boundary frames for readPos (already account for chReadOffset).
    // At these boundaries, each channel is at its own zero crossing.
    int snappedStartFrame { 0 };
    int snappedEndFrame   { 0 };
    bool               pingPongForward { true };
    bool               active    { false };
    bool               noteHeld  { false };
    double             currentSampleRate { 44100.0 };

    // Steal-fade state
    bool  stealing    { false };
    int   stealCountdown { 0 };
    static constexpr int kStealFadeSamples { 64 };
    // Pending retrigger after steal fade
    int              pendingMidiNote { 60 };
    float            pendingVelocity { 1.0f };
    std::shared_ptr<const SampleData> pendingSample;
    VoiceParams       pendingParams;

    juce::SmoothedValue<float> pitchBendSmoothed;
    juce::SmoothedValue<float> portamentoGlide;   // smoothed (midiNote - rootNote) in semitones
    juce::SmoothedValue<float> stealGain;

    // De-click envelope: fades in at note start and out at note end to eliminate
    // boundary clicks when the sample doesn't start/end at zero.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> deClickGain;
    bool deClickingOut { false };
    static constexpr int kDeClickSamples { 441 };  // ~10 ms at 44100 Hz

    // Loop crossfade: when the read head wraps or bounces at a loop boundary,
    // a shadow head continues the old trajectory while the real head starts the
    // new one.  The output crossfades from shadow→real over kLoopXfadeSamples.
    static constexpr int kLoopXfadeSamples { 256 };  // ~6 ms at 44100 Hz
    double loopXfadePos       { 0.0 };   // shadow read position
    int    loopXfadeCounter   { 0 };     // countdown; 0 = inactive
    bool   loopXfadeFwd       { true };  // shadow direction (for ping-pong)

    AdsrEnvelope volumeEnv;
    AdsrEnvelope pitchEnv;
};
