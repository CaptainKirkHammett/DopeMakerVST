#include "SamplerEngine.h"

SamplerEngine::SamplerEngine()
{
    // Initialise default voice params.
    voiceParamsTemplate = VoiceParams{};
}

SamplerEngine::~SamplerEngine() = default;

void SamplerEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (auto& voice : voices)
        voice.prepare (spec);

    sharedPitchLfo.prepare  (spec.sampleRate);
    sharedVolRelLfo.prepare (spec.sampleRate);

    pitchLfoBuffer.resize (static_cast<size_t> (spec.maximumBlockSize), 0.0f);
}

void SamplerEngine::setSampleData (std::shared_ptr<SampleData> newSample)
{
    // Replace the shared_ptr under a SpinLock.  Voices that are already
    // playing hold their own shared_ptr copy (taken at note-on), so the old
    // buffer stays alive until the last voice referencing it finishes.
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    currentSample = std::move (newSample);
}

void SamplerEngine::setPolyMode (bool isPoly, int voiceCount)
{
    monoMode         = !isPoly;
    activeVoiceCount = isPoly ? juce::jlimit (2, kMaxVoices, voiceCount) : 1;
}

void SamplerEngine::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                      juce::MidiBuffer& midiBuffer,
                                      int startSample, int numSamples)
{
    const auto& vp = voiceParamsTemplate;

    // Configure shared LFOs from the current parameter values.
    sharedPitchLfo.setWaveform (static_cast<LfoWaveform> (vp.pitchLfoWaveform));
    sharedPitchLfo.setRate     (vp.pitchLfoEffectiveRateHz);
    sharedPitchLfo.setDepth    (1.0f);   // raw [-1..+1]; depth applied per-voice

    sharedVolRelLfo.setWaveform (static_cast<LfoWaveform> (vp.volRelLfoWaveform));
    sharedVolRelLfo.setRate     (vp.volRelLfoEffectiveRateHz);
    sharedVolRelLfo.setDepth    (1.0f);  // raw [-1..+1]; depth applied per-voice

    // Pre-render per-sample pitch LFO values for the entire block.
    for (int i = 0; i < numSamples; ++i)
        pitchLfoBuffer[static_cast<size_t> (startSample + i)] = sharedPitchLfo.getNextSample();

    // Advance vol release LFO once for the block (block-rate modulation).
    if (numSamples > 1)
        sharedVolRelLfo.advance (numSamples - 1);
    const float volRelLfoSample = sharedVolRelLfo.getNextSample();

    // Write shared LFO outputs to UI atomics unconditionally — no per-voice
    // indirection, no phase sync needed, no blinking.
    pitchLfoOutput.store  (pitchLfoBuffer[static_cast<size_t> (startSample + numSamples - 1)],
                           std::memory_order_relaxed);
    volRelLfoOutput.store (volRelLfoSample, std::memory_order_relaxed);

    // Build the shared LFO state struct for voices.
    SharedLfoState lfoState;
    lfoState.pitchLfoBuffer      = pitchLfoBuffer.data();
    lfoState.pitchLfoDepthSemi   = vp.pitchLfoDepth;
    lfoState.volRelLfoSample     = volRelLfoSample;
    lfoState.volRelLfoDepth      = vp.volRelLfoDepth;
    lfoState.volRelBaseReleaseMs = vp.volReleaseMs;

    auto midiIt  = midiBuffer.findNextSamplePosition (startSample);
    int  blockEnd = startSample + numSamples;

    int currentPos = startSample;

    // Interleave MIDI events with audio rendering.
    for (; midiIt != midiBuffer.cend() && (*midiIt).samplePosition < blockEnd; ++midiIt)
    {
        const auto metadata = *midiIt;
        const int  eventPos = metadata.samplePosition;

        // Render audio up to this MIDI event.
        if (eventPos > currentPos)
        {
            for (auto& voice : voices)
                voice.renderNextBlock (outputBuffer, currentPos, eventPos - currentPos, lfoState);
            currentPos = eventPos;
        }

        handleMidiMessage (metadata.getMessage(), eventPos, outputBuffer);
    }

    // Render the rest of the block.
    if (currentPos < blockEnd)
    {
        for (auto& voice : voices)
            voice.renderNextBlock (outputBuffer, currentPos, blockEnd - currentPos, lfoState);
    }

    // Update playback position for the UI waveform marker.
    // Find the most recently triggered active voice and expose its read position.
    {
        const SamplerVoice* newestVoice = nullptr;
        uint64_t highestTs = 0;
        for (int i = 0; i < activeVoiceCount; ++i)
        {
            if (voices[i].isActive() && voices[i].noteOnTimestamp > highestTs)
            {
                highestTs  = voices[i].noteOnTimestamp;
                newestVoice = &voices[i];
            }
        }
        const float pos = (newestVoice != nullptr)
            ? newestVoice->getNormalizedReadPos()
            : -1.0f;
        playbackPosition.store (pos, std::memory_order_relaxed);
    }
}

void SamplerEngine::handleMidiMessage (const juce::MidiMessage& msg,
                                        int /*sampleOffset*/,
                                        juce::AudioBuffer<float>& /*outputBuffer*/)
{
    if (msg.isNoteOn())
        noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
    else if (msg.isNoteOff())
        noteOff (msg.getNoteNumber());
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        allNotesOff();
    else if (msg.isPitchWheel())
    {
        // Pitch wheel value is -1..+1 in semitone units (multiplied by range in voice).
        const float semitones = (msg.getPitchWheelValue() / 8192.0f) - 1.0f;
        for (auto& voice : voices)
            if (voice.isActive())
                voice.setPitchBend (semitones);
    }
}

void SamplerEngine::noteOn (int midiNote, float velocity)
{
    noteOnAttempts.fetch_add (1, std::memory_order_relaxed);

    // Copy the shared_ptr under the SpinLock so the voice gets its own
    // reference — the buffer cannot be freed while this voice is playing.
    std::shared_ptr<const SampleData> sample;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        sample = currentSample;
    }

    // Apply velocity LFO offset — additive modulation clamped to [0, 1].
    const float velOffset = velLfoOffset.load (std::memory_order_relaxed);
    velocity = juce::jlimit (0.0f, 1.0f, velocity + velOffset);

    // Probability gate: prob=1 means always play, prob=0 means never play.
    // skipChance is the complement so the knob behaves intuitively (max = full play).
    const float prob      = velProbability.load (std::memory_order_relaxed);
    const float skipChance = 1.0f - prob;
    if (skipChance > 0.0f && rng.nextFloat() < skipChance)
    {
        lastNoteReject.store (kProbability, std::memory_order_relaxed);
        noteOnRejections.fetch_add (1, std::memory_order_relaxed);
        DBG ("SamplerEngine::noteOn REJECTED — probability gate (prob="
             + juce::String (prob, 3) + "), note " + juce::String (midiNote));
        return;
    }

    // Record the effective velocity for the UI meter.
    lastPlayedVelocity.store (velocity, std::memory_order_relaxed);
    noteOnGeneration.fetch_add (1, std::memory_order_relaxed);

    // No sample loaded yet — the velocity meter update above keeps the UI
    // responsive, but starting a voice with null sampleData would leave it
    // permanently stuck active (renderSamples returns early without ever
    // setting active = false), silencing all future notes on this instance.
    if (sample == nullptr)
    {
        lastNoteReject.store (kSampleNull, std::memory_order_relaxed);
        noteOnRejections.fetch_add (1, std::memory_order_relaxed);
        DBG ("SamplerEngine::noteOn REJECTED — sample is null, note " + juce::String (midiNote));
        return;
    }

    // Build per-note params from template + LFO offset.
    // startPos / endPos stay at the marker positions so the loop range is stable.
    // initialReadPos carries the LFO-modulated position so only the first read
    // position varies per note.
    VoiceParams p = voiceParamsTemplate;

    // Resolve random direction per note.
    if (p.randomDir)
        p.reversed = rng.nextBool();

    const float lfoOffset = lfoStartOffset.load (std::memory_order_relaxed);
    if (p.reversed)
        p.initialReadPos = juce::jlimit (p.startPos + 0.001f, p.endPos, p.endPos - lfoOffset);
    else
        p.initialReadPos = juce::jlimit (p.startPos, p.endPos - 0.001f, p.startPos + lfoOffset);

    // Slicer: divide [startPos, endPos] into N equal slices; each MIDI note plays one slice.
    if (slicerEnabled.load (std::memory_order_relaxed))
    {
        const int  loNote     = slicerLoNote.load (std::memory_order_relaxed);
        const int  numSlices  = sliceCount.load   (std::memory_order_relaxed);
        const bool randomMode = slicerRandomEnabled.load (std::memory_order_relaxed);

        if (randomMode)
        {
            // Slice1: the Slice Start key picks a random slice; all other
            // slice keys play their assigned slice normally.
            if (midiNote < loNote || midiNote >= loNote + numSlices)
            {
                lastNoteReject.store (kSlicerRange, std::memory_order_relaxed);
                noteOnRejections.fetch_add (1, std::memory_order_relaxed);
                DBG ("SamplerEngine::noteOn REJECTED — Slice1 range (note="
                     + juce::String (midiNote) + " range=" + juce::String (loNote)
                     + "-" + juce::String (loNote + numSlices - 1) + ")");
                return;
            }

            const int sliceIdx = (midiNote == loNote)
                ? rng.nextInt (numSlices)
                : juce::jlimit (0, numSlices - 1, midiNote - loNote);

            const float origStart  = voiceParamsTemplate.startPos;
            const float origEnd    = voiceParamsTemplate.endPos;
            const float sliceWidth = (origEnd - origStart) / static_cast<float> (numSlices);
            p.startPos = origStart + static_cast<float> (sliceIdx)     * sliceWidth;
            p.endPos   = origStart + static_cast<float> (sliceIdx + 1) * sliceWidth;
            p.rootNote = midiNote;
        }
        else
        {
            // Normal slicer: notes outside the slice key range are silenced entirely.
            if (midiNote < loNote || midiNote >= loNote + numSlices)
            {
                lastNoteReject.store (kSlicerRange, std::memory_order_relaxed);
                noteOnRejections.fetch_add (1, std::memory_order_relaxed);
                DBG ("SamplerEngine::noteOn REJECTED — slicer range (note="
                     + juce::String (midiNote) + " range=" + juce::String (loNote)
                     + "-" + juce::String (loNote + numSlices - 1) + ")");
                return;
            }

            const int   sliceIdx   = juce::jlimit (0, numSlices - 1, midiNote - loNote);
            const float origStart  = voiceParamsTemplate.startPos;
            const float origEnd    = voiceParamsTemplate.endPos;
            const float sliceWidth = (origEnd - origStart) / static_cast<float> (numSlices);
            p.startPos = origStart + static_cast<float> (sliceIdx)     * sliceWidth;
            p.endPos   = origStart + static_cast<float> (sliceIdx + 1) * sliceWidth;
            p.rootNote = midiNote;
        }
    }

    // Clear rejection status on a successful note-on.
    lastNoteReject.store (kNone, std::memory_order_relaxed);

    // Fire the callback (e.g. LFO phase reset) — on audio thread, must be lock-free.
    if (onNoteOn)
        onNoteOn();

    // In poly mode, only stop voices that are still held — voices already in
    // their ADSR release phase are left to ring out naturally, giving true
    // polyphonic overlap when the same key retriggers (e.g. with changing
    // start position).  In mono mode, stop all matching voices immediately.
    for (int i = 0; i < activeVoiceCount; ++i)
    {
        if (voices[i].isActive() && voices[i].getCurrentMidiNote() == midiNote)
        {
            if (monoMode || voices[i].isNoteHeld())
                voices[i].stopNote (false);
        }
    }

    if (monoMode)
    {
        // Mono: always use voice 0.
        auto& voice = voices[0];
        voice.noteOnTimestamp = ++nextTimestamp;
        if (voice.isActive() && voice.isNoteHeld() && p.portamentoMs > 0.0f)
        {
            // Legato: note is still held — glide pitch without restarting envelope.
            voice.retriggerPitch (midiNote, velocity, p);
        }
        else if (voice.isActive())
        {
            voice.stealAndRestart (midiNote, velocity, sample, p);
        }
        else
        {
            voice.startNote (midiNote, velocity, sample, p);
        }
        return;
    }

    // Poly: find a free voice.
    SamplerVoice* target = findFreeVoice();
    if (target == nullptr)
        target = findOldestVoice();

    if (target == nullptr)
    {
        lastNoteReject.store (kNoVoice, std::memory_order_relaxed);
        noteOnRejections.fetch_add (1, std::memory_order_relaxed);
        DBG ("SamplerEngine::noteOn REJECTED — no free voice, note " + juce::String (midiNote));
        return;
    }

    target->noteOnTimestamp = ++nextTimestamp;

    if (target->isActive())
        target->stealAndRestart (midiNote, velocity, sample, p);
    else
        target->startNote (midiNote, velocity, sample, p);
}

void SamplerEngine::noteOff (int midiNote)
{
    for (int i = 0; i < activeVoiceCount; ++i)
    {
        if (voices[i].isActive() && voices[i].getCurrentMidiNote() == midiNote)
            voices[i].stopNote (true);
    }
}

void SamplerEngine::allNotesOff()
{
    for (auto& voice : voices)
        voice.stopNote (true);
}

void SamplerEngine::forceAllVoicesOff()
{
    for (auto& voice : voices)
        voice.forceOff();
}

SamplerVoice* SamplerEngine::findFreeVoice() const
{
    for (int i = 0; i < activeVoiceCount; ++i)
        if (!voices[i].isActive())
            return const_cast<SamplerVoice*> (&voices[i]);
    return nullptr;
}

SamplerVoice* SamplerEngine::findOldestVoice() const
{
    SamplerVoice* oldest     = nullptr;
    uint64_t      lowestTime = std::numeric_limits<uint64_t>::max();

    for (int i = 0; i < activeVoiceCount; ++i)
    {
        if (voices[i].noteOnTimestamp < lowestTime)
        {
            lowestTime = voices[i].noteOnTimestamp;
            oldest     = const_cast<SamplerVoice*> (&voices[i]);
        }
    }

    return oldest;
}
