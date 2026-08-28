#include "ArpeggiatorProcessor.h"
#include <algorithm>

void ArpeggiatorProcessor::prepare (double sr)
{
    sampleRate = sr;
    lfoLen.prepare (sr);
    lfoLen.setTempoSync (true);
    lfoLen.setDepth (1.0f);
    reset();
}

void ArpeggiatorProcessor::reset()
{
    heldNotes.clear();
    arpSequence.clear();
    dirty          = true;
    currentIndex   = 0;
    stepCounter    = 0;
    activeNote     = -1;
    noteActive     = false;
    noteOffCounter = 0;
    noteOffAt      = 0;
    tieToNext      = false;
    lfoLen.resetPhase();
}

void ArpeggiatorProcessor::resetTiming()
{
    currentIndex   = 0;
    stepCounter    = 0;
    noteOffCounter = 0;
    noteOffAt      = 0;
    tieToNext      = false;
    dirty          = true;   // rebuild sequence on next process() call
    lfoLen.resetPhase();
}

void ArpeggiatorProcessor::rebuildArpSequence (int octaves, ArpDirection dir)
{
    arpSequence.clear();
    if (heldNotes.empty()) return;

    for (int oct = 0; oct < octaves; ++oct)
        for (int note : heldNotes)
            arpSequence.push_back (juce::jlimit (0, 127, note + oct * 12));

    if (dir == ArpDirection::Down)
        std::reverse (arpSequence.begin(), arpSequence.end());
    // ArpDirection::Random: sequence kept in Up order; a random index is
    // chosen at each step boundary, so no pre-shuffle needed.
}

void ArpeggiatorProcessor::process (juce::MidiBuffer& midi, int numSamples,
                                     double bpm, SyncDivision div, int octaves,
                                     ArpDirection dir, float noteLengthFrac,
                                     float probability, bool enabled,
                                     bool lfoActive, LfoWaveform lfoWave, SyncDivision lfoDiv,
                                     float lfoDepth, float tieProb)
{
    // ── Track held notes from the incoming MIDI stream ────────────────────────
    const bool wasEmpty = heldNotes.empty();

    for (const auto& meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            if (std::find (heldNotes.begin(), heldNotes.end(), note) == heldNotes.end())
            {
                heldNotes.push_back (note);
                std::sort (heldNotes.begin(), heldNotes.end());
                dirty = true;
            }
        }
        else if (msg.isNoteOff())
        {
            const int  note = msg.getNoteNumber();
            const auto it   = std::find (heldNotes.begin(), heldNotes.end(), note);
            if (it != heldNotes.end())
            {
                heldNotes.erase (it);
                dirty = true;
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            heldNotes.clear();
            dirty = true;
        }
    }

    // Restart step and sequence when notes are first pressed after a rest.
    if (wasEmpty && !heldNotes.empty())
    {
        stepCounter  = 0;
        currentIndex = 0;
    }

    // ── Disabled: pass MIDI unchanged, stop any in-flight arp note ────────────
    if (!enabled)
    {
        if (noteActive)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, activeNote), 0);
            noteActive = false;
            activeNote = -1;
        }
        return;
    }

    // ── Enabled: replace note MIDI with arpeggiated events ───────────────────
    juce::MidiBuffer arpMidi;

    // Pass non-note MIDI (CC, pitch bend, aftertouch…) through unchanged.
    for (const auto& meta : midi)
    {
        const auto msg = meta.getMessage();
        if (!msg.isNoteOn() && !msg.isNoteOff())
            arpMidi.addEvent (msg, meta.samplePosition);
    }

    // Rebuild sequence if held notes, octaves, or direction changed.
    if (dirty || octaves != cachedOctaves || dir != cachedDir)
    {
        rebuildArpSequence (octaves, dir);
        if (!arpSequence.empty())
            currentIndex = currentIndex % (int) arpSequence.size();
        else
            currentIndex = 0;
        cachedOctaves = octaves;
        cachedDir     = dir;
        dirty         = false;
    }

    // Nothing to play — stop any lingering note and exit.
    if (arpSequence.empty())
    {
        if (noteActive)
        {
            arpMidi.addEvent (juce::MidiMessage::noteOff (1, activeNote), 0);
            noteActive = false;
            activeNote = -1;
        }
        midi = arpMidi;
        return;
    }

    // Configure the note-length LFO every block so parameter changes take effect.
    if (lfoActive)
    {
        lfoLen.setWaveform (lfoWave);
        lfoLen.setSyncedRate (bpm, lfoDiv);
    }

    const float stepHz       = LfoProcessor::syncDivisionToHz (bpm, div);
    const int samplesPerStep = juce::jmax (1, (int) (sampleRate / stepHz));

    int pos = 0;
    while (pos < numSamples)
    {
        // Handle note-off if its deadline has been reached.
        if (noteActive && noteOffCounter >= noteOffAt)
        {
            arpMidi.addEvent (juce::MidiMessage::noteOff (1, activeNote), pos);
            noteActive = false;
            activeNote = -1;
        }

        // Fire the next arp step if we're at a step boundary.
        if (stepCounter >= samplesPerStep)
        {
            stepCounter -= samplesPerStep;

            // Tie: if the previous note was marked to tie into this step, extend
            // it without retriggering — the note just keeps playing for one more step.
            if (tieToNext && noteActive)
            {
                tieToNext  = false;
                // Extend the note's off deadline past this step boundary.
                noteOffAt  = noteOffCounter + samplesPerStep;
                // Silently advance the sequence index.
                if (dir != ArpDirection::Random)
                    currentIndex = (currentIndex + 1) % (int) arpSequence.size();
            }
            else
            {
                tieToNext = false;

                // Pick the note: sequential for Up/Down, random for Random.
                int noteIdx;
                if (dir == ArpDirection::Random)
                {
                    noteIdx = rng.nextInt ((int) arpSequence.size());
                }
                else
                {
                    noteIdx      = currentIndex;
                    currentIndex = (currentIndex + 1) % (int) arpSequence.size();
                }

                const int note = arpSequence[noteIdx];

                // Probability gate: skip this step if roll fails.
                if (rng.nextFloat() < probability)
                {
                    if (noteActive)
                    {
                        arpMidi.addEvent (juce::MidiMessage::noteOff (1, activeNote), pos);
                        noteActive = false;
                    }

                    // Per-note length: base value optionally modulated by the LFO.
                    float effectiveLen = noteLengthFrac;
                    if (lfoActive)
                    {
                        const float lfoVal = lfoLen.getNextSample();  // advances LFO by 1 sample
                        effectiveLen = juce::jlimit (0.01f, 0.99f, noteLengthFrac + lfoVal * lfoDepth * 0.5f);
                    }
                    noteOffAt = juce::jmax (1, (int) (samplesPerStep * effectiveLen));

                    arpMidi.addEvent (
                        juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), pos);
                    activeNote     = note;
                    noteActive     = true;
                    noteOffCounter = 0;

                    // Decide whether this note ties into the next step.
                    // Extend noteOffAt so the note survives until the next boundary.
                    if (tieProb > 0.0f && rng.nextFloat() < tieProb)
                    {
                        tieToNext = true;
                        noteOffAt = samplesPerStep + 1;  // keep note alive through next boundary
                    }
                }
            }
        }

        // Advance to the next event (step boundary or note-off), clamped to block end.
        const int t1      = samplesPerStep - stepCounter;
        const int t2      = noteActive ? (noteOffAt - noteOffCounter) : (numSamples - pos);
        const int advance = juce::jmin (juce::jmax (1, juce::jmin (t1, t2)), numSamples - pos);

        stepCounter += advance;
        if (noteActive) noteOffCounter += advance;
        if (lfoActive)  lfoLen.advance (advance);  // keep LFO synced to time between note-ons
        pos += advance;
    }

    midi = arpMidi;
}
