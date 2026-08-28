#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "LfoProcessor.h"   // SyncDivision + syncDivisionToHz

enum class ArpDirection { Up, Down, Random };

// Processes a MidiBuffer in-place before it reaches the sampler engine.
// When enabled: tracks held notes, suppresses raw note-on/off, and emits
// arpeggiated note events at tempo-synced step boundaries.
// When disabled: passes all MIDI through unchanged.
class ArpeggiatorProcessor
{
public:
    ArpeggiatorProcessor() = default;

    void prepare (double sampleRate);
    void reset   ();
    void resetTiming ();  // resets step/sequence position without clearing held notes

    void process (juce::MidiBuffer& midi, int numSamples,
                  double bpm, SyncDivision div, int octaves,
                  ArpDirection dir, float noteLengthFrac,
                  float probability, bool enabled,
                  bool lfoActive, LfoWaveform lfoWave, SyncDivision lfoDiv,
                  float lfoDepth, float tieProb);

private:
    void rebuildArpSequence (int octaves, ArpDirection dir);

    double           sampleRate    { 44100.0 };
    std::vector<int> heldNotes;     // sorted, unique MIDI note numbers held
    std::vector<int> arpSequence;   // playback sequence after octave expansion
    bool             dirty         { true };

    int          currentIndex   { 0 };   // next step index in arpSequence
    int          stepCounter    { 0 };   // samples elapsed in the current step
    int          activeNote     { -1 };  // note currently sent to engine (-1 = none)
    bool         noteActive     { false };
    int          noteOffCounter { 0 };   // samples elapsed since the active note-on

    int          noteOffAt      { 0 };   // per-note note-off deadline (set at each note-on)
    bool         tieToNext      { false }; // if true, the active note continues into the next step

    int          cachedOctaves  { 1 };
    ArpDirection cachedDir      { ArpDirection::Up };

    LfoProcessor lfoLen;          // LFO that modulates note length
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArpeggiatorProcessor)
};
