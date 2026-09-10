#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "SamplerVoice.h"

class SamplerEngine
{
public:
    static constexpr int kMaxVoices { 16 };

    SamplerEngine();
    ~SamplerEngine();

    void prepare (const juce::dsp::ProcessSpec& spec);

    // Called from the main thread after loading a new sample.
    // Ownership of SampleData transfers to engine.
    void setSampleData (std::shared_ptr<SampleData> newSample);

    // Main audio render entry point; processes MIDI and renders all active voices.
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          juce::MidiBuffer& midiBuffer,
                          int startSample, int numSamples);

    // Called from PluginProcessor (main thread) when polyphony settings change.
    void setPolyMode (bool isPoly, int voiceCount);

    // Hard-kill all voices — clears every internal flag (steal, de-click, envelope).
    // Called on transport jumps where audio is already discontinuous.
    void forceAllVoicesOff();

    // Callback invoked on every note-on, on the audio thread.
    // PluginProcessor sets this to trigger LFO phase reset when tempo-sync is active.
    std::function<void()> onNoteOn;

    // Live parameters read by the engine at note-on time; set by PluginProcessor.
    VoiceParams voiceParamsTemplate;

    // Current LFO-modulated start position offset (added to voiceParamsTemplate.startPos).
    std::atomic<float> lfoStartOffset { 0.0f };

    // Slicer mode: divides [startPos, endPos] into N equal slices; each MIDI note plays one.
    std::atomic<bool>  slicerEnabled { false };
    std::atomic<int>   slicerLoNote  { 60 };   // MIDI note for first slice (Slice Start key, C3)
    std::atomic<int>   sliceCount    { 16 };   // Number of slices (1–128)
    std::atomic<bool>  slicerRandomEnabled { false }; // Slice1: only trigger note plays a random slice

    // Current velocity LFO offset [-1, 1]; added to incoming note velocity at note-on.
    std::atomic<float> velLfoOffset { 0.0f };

    // Probability [0, 1] that any note-on plays (1 = always play, 0 = never play).
    std::atomic<float> velProbability { 1.0f };

    // Normalised playback position (0..1) of the most recently triggered active voice.
    // -1.0f = no voice active.  Written on the audio thread, read by the UI timer.
    std::atomic<float> playbackPosition { -1.0f };

    // Shared LFO outputs in [-1, +1] for UI display.  Written unconditionally
    // each block from the single shared LFO instances.
    std::atomic<float> pitchLfoOutput  { 0.0f };
    std::atomic<float> volRelLfoOutput { 0.0f };

    // Effective velocity (after LFO offset) of the last note that passed the probability gate.
    // -1.0f = no note played yet.  noteOnGeneration increments on every such note.
    std::atomic<float>    lastPlayedVelocity { -1.0f };
    std::atomic<uint32_t> noteOnGeneration   { 0 };

    // Diagnostics: reason the last note-on was rejected (0 = not rejected).
    // Written on the audio thread, read by the UI for troubleshooting.
    enum NoteRejectReason : int { kNone = 0, kProbability = 1, kSampleNull = 2, kSlicerRange = 3, kNoVoice = 4 };
    std::atomic<int>      lastNoteReject     { kNone };
    std::atomic<uint32_t> noteOnAttempts     { 0 };   // total note-on messages received
    std::atomic<uint32_t> noteOnRejections   { 0 };   // total rejected

private:
    void handleMidiMessage (const juce::MidiMessage& msg, int sampleOffset,
                            juce::AudioBuffer<float>& outputBuffer);
    void noteOn  (int midiNote, float velocity);
    void noteOff (int midiNote);
    void allNotesOff();

    SamplerVoice* findFreeVoice() const;
    SamplerVoice* findOldestVoice() const;

    std::array<SamplerVoice, kMaxVoices> voices;

    // Shared sample pointer.  Main thread writes via setSampleData();
    // audio thread reads via noteOn() and passes copies to voices.
    // Voices hold their own shared_ptr so the buffer stays alive for the
    // duration of any note even when a new sample is loaded mid-play.
    // Protected by sampleLock (SpinLock is audio-thread safe).
    juce::SpinLock sampleLock;
    std::shared_ptr<SampleData> currentSample;

    int  activeVoiceCount { 8 };
    bool monoMode         { false };

    double sampleRate { 44100.0 };
    juce::Random rng;

    uint64_t nextTimestamp { 0 };

    // Shared LFO instances — one each for the entire engine.
    LfoProcessor sharedPitchLfo;
    LfoProcessor sharedVolRelLfo;

    // Pre-rendered per-sample pitch LFO buffer (sized in prepare()).
    std::vector<float> pitchLfoBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEngine)
};
