#include "SamplerVoice.h"
#include <cmath>

// Finds the frame within ±searchRadius of origin where the amplitude of a
// single channel is lowest — used to snap each channel independently.
// preferForward=true  → prefers forward candidates (start: don't go before marker).
// preferForward=false → prefers backward candidates (end: don't overshoot marker).
static int findNearestZeroCrossing (const juce::AudioBuffer<float>& buf,
                                    int origin, int searchRadius,
                                    bool preferForward, int channel)
{
    const int    frames = buf.getNumSamples();
    const int    ch     = juce::jlimit (0, buf.getNumChannels() - 1, channel);
    const float* data   = buf.getReadPointer (ch);

    const int originClamped = juce::jlimit (0, frames - 1, origin);
    int   bestFrame = originClamped;
    float bestAmp   = std::abs (data[originClamped]);

    if (bestAmp < 1e-4f)
        return bestFrame;

    for (int d = 1; d <= searchRadius; ++d)
    {
        const int fwd  = origin + d;
        const int back = origin - d;

        auto check = [&] (int f)
        {
            if (f < 0 || f >= frames) return;
            const float amp = std::abs (data[f]);
            if (amp < bestAmp) { bestAmp = amp; bestFrame = f; }
        };

        if (preferForward) { check (fwd);  check (back); }
        else               { check (back); check (fwd);  }

        if (bestAmp < 1e-4f)
            break;
    }

    return bestFrame;
}

void SamplerVoice::prepare (const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    volumeEnv.prepare (spec.sampleRate);
    pitchEnv.prepare  (spec.sampleRate);

    pitchBendSmoothed.reset (spec.sampleRate, 0.005);  // 5 ms smoothing
    pitchBendSmoothed.setCurrentAndTargetValue (0.0f);

    portamentoGlide.reset (spec.sampleRate, 0.0);
    portamentoGlide.setCurrentAndTargetValue (0.0f);


    deClickGain.reset (spec.sampleRate, static_cast<double> (kDeClickSamples) / spec.sampleRate);
    deClickGain.setCurrentAndTargetValue (1.0f);
}

void SamplerVoice::startNote (int note, float vel,
                               std::shared_ptr<const SampleData> sample,
                               const VoiceParams& p)
{
    sampleData = std::move (sample);
    params     = p;
    midiNote   = note;
    velocity   = vel;

    // Calculate start position in frames; reversed playback begins at the end point.
    // Snap to the nearest zero crossing within a small search window so the first
    // output sample is at/near zero — eliminates the click that the de-click fade
    // alone cannot prevent when the waveform starts at high amplitude.
    static constexpr int kZcSearchRadius = 1024;   // ~23 ms at 44100 Hz
    const int totalFrames = (sampleData != nullptr) ? sampleData->buffer.getNumSamples() : 0;
    const int numSrcChans = (sampleData != nullptr) ? sampleData->buffer.getNumChannels() : 1;

    // Find independent zero-crossing frames per channel for both boundaries.
    // chReadOffset[ch] = offset added to readPos when reading that channel, so
    // each channel starts and ends at its own zero crossing.
    {
        const int rawStart = static_cast<int> (totalFrames * p.startPos);
        const int rawEnd   = static_cast<int> (totalFrames * p.endPos);

        int zcStart[kMaxChannels], zcEnd[kMaxChannels];
        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            const int srcCh = juce::jmin (ch, numSrcChans - 1);
            if (sampleData != nullptr)
            {
                zcStart[ch] = findNearestZeroCrossing (sampleData->buffer, rawStart, kZcSearchRadius, true,  srcCh);
                zcEnd[ch]   = findNearestZeroCrossing (sampleData->buffer, rawEnd,   kZcSearchRadius, false, srcCh);
            }
            else
            {
                zcStart[ch] = rawStart;
                zcEnd[ch]   = rawEnd;
            }
        }

        if (p.perChannelZC)
        {
            // Per-channel mode: each channel reads from its own ZC offset so both
            // start and end boundaries are click-free per channel.
            const int refStart = *std::min_element (zcStart, zcStart + kMaxChannels);
            for (int ch = 0; ch < kMaxChannels; ++ch)
                chReadOffset[ch] = zcStart[ch] - refStart;

            int refEnd = zcEnd[0] - chReadOffset[0];
            for (int ch = 1; ch < kMaxChannels; ++ch)
                refEnd = juce::jmin (refEnd, zcEnd[ch] - chReadOffset[ch]);

            snappedStartFrame = refStart;
            snappedEndFrame   = refEnd;
        }
        else
        {
            // Shared mode: use the earliest ZC across channels for both boundaries
            // so L and R are always in sync (no stereo timing offset).
            for (int ch = 0; ch < kMaxChannels; ++ch)
                chReadOffset[ch] = 0;

            snappedStartFrame = *std::min_element (zcStart, zcStart + kMaxChannels);
            snappedEndFrame   = *std::min_element (zcEnd,   zcEnd   + kMaxChannels);
        }

        // Guard: ZC snapping can overshoot when the playable region is small
        // (e.g. LFO offset pushing startPos close to endPos).  Fall back to
        // raw frame positions if the snapped boundaries are inverted or too close.
        static constexpr int kMinRegionFrames = 64;
        if (snappedEndFrame - snappedStartFrame < kMinRegionFrames)
        {
            snappedStartFrame = rawStart;
            snappedEndFrame   = rawEnd;
            for (int ch = 0; ch < kMaxChannels; ++ch)
                chReadOffset[ch] = 0;

            // If even the raw region is degenerate, don't activate this voice.
            if (snappedEndFrame - snappedStartFrame < kMinRegionFrames)
            {
                active = false;
                return;
            }
        }
    }

    // Determine initial read position.  If initialReadPos is set (>= 0), it
    // carries an LFO-modulated start that may differ from the loop boundaries.
    // ZC-snap it independently and clamp within the loop region.
    if (p.reversed)
    {
        if (p.initialReadPos >= 0.0f)
        {
            int rawInitial = static_cast<int> (totalFrames * p.initialReadPos);
            if (sampleData != nullptr)
                rawInitial = findNearestZeroCrossing (sampleData->buffer, rawInitial, kZcSearchRadius, false, 0);
            readPos = static_cast<double> (juce::jlimit (snappedStartFrame, snappedEndFrame, rawInitial));
        }
        else
        {
            readPos = static_cast<double> (snappedEndFrame);
        }
        pingPongForward = false;
    }
    else
    {
        if (p.initialReadPos >= 0.0f)
        {
            int rawInitial = static_cast<int> (totalFrames * p.initialReadPos);
            if (sampleData != nullptr)
                rawInitial = findNearestZeroCrossing (sampleData->buffer, rawInitial, kZcSearchRadius, true, 0);
            readPos = static_cast<double> (juce::jlimit (snappedStartFrame, snappedEndFrame, rawInitial));
        }
        else
        {
            readPos = static_cast<double> (snappedStartFrame);
        }
        pingPongForward = true;
    }
    active            = true;
    noteHeld          = true;
    stealing          = false;
    stealCountdown    = 0;
    deClickingOut     = false;
    loopXfadeCounter  = 0;

    // Fade in from silence to avoid a click when the sample starts mid-waveform.
    // Skip the fade when playback begins at the sample's natural start (frame 0)
    // — the sample already has its own attack or built-in fade there.
    if (readPos > 1.0)
    {
        deClickGain.setCurrentAndTargetValue (0.0f);
        deClickGain.setTargetValue (1.0f);
    }
    else
    {
        deClickGain.setCurrentAndTargetValue (1.0f);
    }

    // Set up portamento glide.  Glide only applies in legato mode — arpeggiated
    // and stolen voices always jump instantly to the new pitch.
    {
        const float targetOffset  = static_cast<float> (note - p.rootNote);
        const double glideTimeSec = (p.portamentoMs > 0.0f)
                                    ? static_cast<double> (p.portamentoMs) / 1000.0
                                    : 0.0;
        portamentoGlide.reset (currentSampleRate, glideTimeSec);
        if (glideTimeSec <= 0.0)
            portamentoGlide.setCurrentAndTargetValue (targetOffset);
        else
            portamentoGlide.setTargetValue (targetOffset);
    }

    volumeEnv.setParameters (p.volAttackMs, p.volDecayMs, p.volSustain, p.volReleaseMs);
    pitchEnv.setParameters  (p.pitchAttackMs, p.pitchDecayMs, p.pitchSustain, p.pitchReleaseMs);
    volumeEnv.noteOn();
    pitchEnv.noteOn();

}

void SamplerVoice::stopNote (bool allowTailOff)
{
    noteHeld = false;

    if (!active)
        return;

    if (allowTailOff)
    {
        volumeEnv.noteOff();
        pitchEnv.noteOff();
    }
    else
    {
        // De-click fade-out: ramp to silence over kDeClickSamples instead of
        // cutting immediately — eliminates clicks on hard stops and note stealing.
        deClickingOut = true;
        deClickGain.setTargetValue (0.0f);
    }
}

void SamplerVoice::forceOff()
{
    active           = false;
    noteHeld         = false;
    stealing         = false;
    stealCountdown   = 0;
    deClickingOut    = false;
    loopXfadeCounter = 0;
    volumeEnv.reset();
    pitchEnv.reset();
    sampleData    = nullptr;
    pendingSample = nullptr;
}

void SamplerVoice::stealAndRestart (int newMidiNote, float newVelocity,
                                     std::shared_ptr<const SampleData> newSample,
                                     const VoiceParams& newParams)
{
    // Store pending note data; begin steal fade-out.
    pendingMidiNote  = newMidiNote;
    pendingVelocity  = newVelocity;
    pendingSample    = std::move (newSample);
    pendingParams    = newParams;

    // Cancel any in-progress de-click fade so it doesn't deactivate the voice
    // before the steal countdown completes (the steal fade handles the crossfade).
    deClickingOut = false;

    stealing       = true;
    stealCountdown = kStealFadeSamples;
}

void SamplerVoice::retriggerPitch (int newMidiNote, float newVelocity, const VoiceParams& p)
{
    midiNote = newMidiNote;
    velocity = newVelocity;
    noteHeld = true;
    params   = p;

    const float targetOffset  = static_cast<float> (newMidiNote - p.rootNote);
    const double glideTimeSec = static_cast<double> (p.portamentoMs) / 1000.0;
    portamentoGlide.reset (currentSampleRate, glideTimeSec);
    if (p.portamentoMs <= 0.0f)
        portamentoGlide.setCurrentAndTargetValue (targetOffset);
    else
        portamentoGlide.setTargetValue (targetOffset);
}

void SamplerVoice::setPitchBend (float semitones)
{
    pitchBendSmoothed.setTargetValue (semitones);
}

bool SamplerVoice::isActive() const
{
    return active;
}

float SamplerVoice::getNormalizedReadPos() const
{
    if (sampleData == nullptr || sampleData->buffer.getNumSamples() == 0)
        return 0.0f;
    return static_cast<float> (readPos) / static_cast<float> (sampleData->buffer.getNumSamples());
}

// pitchEnvSample and pitchLfoSemi must be pre-computed by the caller (advance envelopes/LFOs exactly once per sample).
float SamplerVoice::computePlaybackRatio (float pitchEnvSample, float pitchLfoSemi) const
{
    if (sampleData == nullptr)
        return 1.0f;

    const float noteOffset     = portamentoGlide.getCurrentValue()
                                 + static_cast<float> (params.semitoneOffset);
    const float pitchBendSemi  = pitchBendSmoothed.getCurrentValue()
                                 * static_cast<float> (params.pitchBendRange);
    const float pitchEnvSemi   = pitchEnvSample * params.pitchEnvDepth;
    const float totalSemitones = noteOffset + pitchBendSemi + pitchEnvSemi + pitchLfoSemi;

    // Clamp total pitch shift to ±96 semitones (8 octaves) to prevent the voice
    // from racing through the sample in one step when the LFO pushes pitch to
    // inaudible extremes — without this the voice dies instantly and the user
    // hears silence even though MIDI is active.
    const float clampedSemitones = juce::jlimit (-96.0f, 96.0f, totalSemitones);

    // Fast semitone-to-ratio: exp(n * ln2/12) ≈ exp(n * 0.057762f)
    const float pitchRatio = std::exp (clampedSemitones * 0.057762f);
    const float srRatio    = static_cast<float> (sampleData->sourceSampleRate / currentSampleRate);
    return pitchRatio * srRatio;
}

float SamplerVoice::readSampleHermite (float pos, int channel) const
{
    if (sampleData == nullptr)
        return 0.0f;

    const auto& buf    = sampleData->buffer;
    const int   frames = buf.getNumSamples();
    const int   ch     = juce::jlimit (0, buf.getNumChannels() - 1, channel);

    const int   i1   = static_cast<int> (pos);
    const float frac = pos - static_cast<float> (i1);

    if (i1 < 1 || i1 + 2 >= frames)
    {
        // Fall back to linear at boundaries where we lack 4 neighbors.
        const int i0 = i1;
        const int iN = i0 + 1;
        if (i0 < 0 || iN >= frames)
            return 0.0f;
        const float* data = buf.getReadPointer (ch);
        return data[i0] + frac * (data[iN] - data[i0]);
    }

    const float* data = buf.getReadPointer (ch);
    const float y0 = data[i1 - 1];
    const float y1 = data[i1];
    const float y2 = data[i1 + 1];
    const float y3 = data[i1 + 2];

    // Hermite 4-point cubic interpolation.
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

float SamplerVoice::readSampleSinc (float pos, int channel) const
{
    if (sampleData == nullptr)
        return 0.0f;

    const auto& buf    = sampleData->buffer;
    const int   frames = buf.getNumSamples();
    const int   ch     = juce::jlimit (0, buf.getNumChannels() - 1, channel);

    const int   iCenter = static_cast<int> (pos);
    const float frac    = pos - static_cast<float> (iCenter);

    // 8-point windowed sinc: 4 taps each side of the fractional position.
    static constexpr int kHalfTaps = 4;
    const int firstTap = iCenter - kHalfTaps + 1;
    const int lastTap  = iCenter + kHalfTaps;

    if (firstTap < 0 || lastTap >= frames)
        return readSampleHermite (pos, channel);

    const float* data = buf.getReadPointer (ch);
    float result = 0.0f;

    for (int t = firstTap; t <= lastTap; ++t)
    {
        const float d = frac + static_cast<float> (iCenter - t);
        float sincVal;

        if (std::abs (d) < 1.0e-6f)
        {
            sincVal = 1.0f;
        }
        else
        {
            // Normalized sinc: sin(pi*d) / (pi*d)
            const float pid = juce::MathConstants<float>::pi * d;
            sincVal = std::sin (pid) / pid;

            // Blackman-Harris window over the kernel span.
            const float n = d + static_cast<float> (kHalfTaps);
            const float nNorm = n / static_cast<float> (2 * kHalfTaps);
            const float twoPiN  = juce::MathConstants<float>::twoPi * nNorm;
            const float fourPiN = 2.0f * twoPiN;
            const float sixPiN  = 3.0f * twoPiN;
            const float window  = 0.35875f
                                - 0.48829f * std::cos (twoPiN)
                                + 0.14128f * std::cos (fourPiN)
                                - 0.01168f * std::cos (sixPiN);
            sincVal *= window;
        }

        result += data[t] * sincVal;
    }

    return result;
}

float SamplerVoice::readSample (float pos, int channel) const
{
    if (params.hqInterpolation)
        return readSampleSinc (pos, channel);
    return readSampleHermite (pos, channel);
}

void SamplerVoice::handleLoopBoundary()
{
    if (sampleData == nullptr || deClickingOut)
        return;

    const float startFrame = static_cast<float> (snappedStartFrame);
    const float endFrame   = static_cast<float> (snappedEndFrame);

    // Degenerate region: start >= end means the voice has nothing to play.
    // Fade out immediately rather than looping forever at the same position.
    if (snappedEndFrame <= snappedStartFrame)
    {
        deClickingOut = true;
        deClickGain.setTargetValue (0.0f);
        return;
    }

    if (params.looping)
    {
        if (params.loopMode == LoopMode::Forward)
        {
            // Reversed forward-loop wraps at the start boundary back to the end.
            if (params.reversed)
            {
                if (readPos <= startFrame)
                {
                    loopXfadePos     = readPos;
                    loopXfadeFwd     = false;
                    loopXfadeCounter = kLoopXfadeSamples;
                    readPos = endFrame - (startFrame - readPos);
                }
            }
            else
            {
                if (readPos >= endFrame)
                {
                    loopXfadePos     = readPos;
                    loopXfadeFwd     = true;
                    loopXfadeCounter = kLoopXfadeSamples;
                    readPos = startFrame + (readPos - endFrame);
                }
            }
        }
        else // PingPong — direction flips at both boundaries regardless of reversed
        {
            if (pingPongForward && readPos >= endFrame)
            {
                loopXfadePos     = readPos;
                loopXfadeFwd     = true;
                loopXfadeCounter = kLoopXfadeSamples;
                readPos = endFrame - (readPos - endFrame);
                pingPongForward = false;
            }
            else if (!pingPongForward && readPos <= startFrame)
            {
                loopXfadePos     = readPos;
                loopXfadeFwd     = false;
                loopXfadeCounter = kLoopXfadeSamples;
                readPos = startFrame + (startFrame - readPos);
                pingPongForward = true;
            }
        }
    }
    else
    {
        // Reversed one-shot stops when the read head passes the start boundary.
        if (params.reversed)
        {
            if (readPos <= startFrame)
            {
                deClickingOut = true;
                deClickGain.setTargetValue (0.0f);
            }
        }
        else
        {
            if (readPos >= endFrame)
            {
                deClickingOut = true;
                deClickGain.setTargetValue (0.0f);
            }
        }
    }
}

void SamplerVoice::renderSamples (juce::AudioBuffer<float>& outputBuffer,
                                   int startSample, int numSamples,
                                   const SharedLfoState& lfoState)
{
    // Deactivate rather than silently returning — a voice stuck active with
    // null sampleData would block voice allocation and swallow all future notes.
    if (sampleData == nullptr)
    {
        active   = false;
        noteHeld = false;
        return;
    }
    if (!active)
        return;

    const int numOutputChannels = outputBuffer.getNumChannels();
    const int numSrcChannels    = sampleData->buffer.getNumChannels();

    for (int s = 0; s < numSamples; ++s)
    {
        if (!active)
            break;

        pitchBendSmoothed.skip (1);
        portamentoGlide.skip (1);
        const float pitchEnvSample = pitchEnv.getNextSample();
        const float pitchLfoSemi   = lfoState.pitchLfoBuffer[startSample + s]
                                     * lfoState.pitchLfoDepthSemi;
        const float ratio          = computePlaybackRatio (pitchEnvSample, pitchLfoSemi);

        const float volEnvGain  = volumeEnv.getNextSample();
        float stealFactor = 1.0f;
        if (stealing)
        {
            // Smoothstep fade-out: equal-power curve keeps perceived loudness
            // constant through the crossfade, reducing clicks during rapid retrigger.
            const float t = static_cast<float> (stealCountdown) / static_cast<float> (kStealFadeSamples);
            stealFactor = t * t * (3.0f - 2.0f * t);
        }
        const float deClick     = deClickGain.getNextValue();
        const float gain        = velocity * volEnvGain * stealFactor * deClick;

        // Check if steal fade has completed — retrigger the pending note.
        if (stealing)
        {
            --stealCountdown;
            if (stealCountdown <= 0)
            {
                stealing = false;
                startNote (pendingMidiNote, pendingVelocity, std::move (pendingSample), pendingParams);
            }
        }

        if (!active)
            break;

        // Loop crossfade: blend shadow head (fading out) with real head (fading in).
        const bool xfading = loopXfadeCounter > 0;
        float xfadeIn = 1.0f;
        if (xfading)
        {
            xfadeIn = 1.0f - static_cast<float> (loopXfadeCounter) / static_cast<float> (kLoopXfadeSamples);
            // Equal-power crossfade for perceptually smooth transition.
            xfadeIn = xfadeIn * xfadeIn * (3.0f - 2.0f * xfadeIn);  // smoothstep
        }

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            const int srcCh  = (numSrcChannels > 1) ? ch : 0;
            const int offset = (ch < kMaxChannels) ? chReadOffset[ch] : 0;
            float out = readSample (static_cast<float> (readPos) + offset, srcCh);

            if (xfading)
            {
                const float shadow = readSample (static_cast<float> (loopXfadePos) + offset, srcCh);
                out = out * xfadeIn + shadow * (1.0f - xfadeIn);
            }

            outputBuffer.addSample (ch, startSample + s, out * gain);
        }

        if (pingPongForward)
            readPos += ratio;
        else
            readPos -= ratio;

        // Advance shadow head in its original direction during crossfade.
        if (xfading)
        {
            if (loopXfadeFwd)
                loopXfadePos += ratio;
            else
                loopXfadePos -= ratio;
            --loopXfadeCounter;
        }

        // Guard against NaN/Inf — a corrupted readPos makes all boundary
        // comparisons fail (NaN is never >= or <=), leaving the voice stuck
        // active but silent forever, blocking its voice slot.
        if (std::isnan (readPos) || std::isinf (readPos))
        {
            active   = false;
            noteHeld = false;
            volumeEnv.reset();
            pitchEnv.reset();
            break;
        }

        handleLoopBoundary();

        // While a steal is in progress the voice must stay active so the
        // countdown can complete and startNote() fires for the pending note.
        // Skipping these checks prevents the envelope release or de-click
        // fade from prematurely killing voice[0] in mono mode.
        if (!stealing)
        {
            // Stop voice once the de-click fade-out reaches silence.
            if (deClickingOut && !deClickGain.isSmoothing())
            {
                active = false;
                volumeEnv.reset();
                pitchEnv.reset();
                break;
            }

            // Stop voice when volume envelope finishes (non-looping or after note-off).
            if (!volumeEnv.isActive())
            {
                active = false;
                break;
            }
        }
    }
}

void SamplerVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                     int startSample, int numSamples,
                                     const SharedLfoState& lfoState)
{
    if (!active)
        return;

    // Modulate the volume envelope's release time from the shared vol release LFO.
    // Bipolar multiplicative: positive lfoOut lengthens release, negative shortens it.
    // The negative side gets a 1.5× boost so random draws around -0.67 reliably reach
    // very short release values — without this, uniform random targets rarely land near
    // -1.0 and the floor is almost never touched.  Floor lowered to 3ms so those short
    // releases are audible.
    const float lfoOut     = lfoState.volRelLfoSample;
    const float boostedLfo = lfoOut < 0.0f ? lfoOut * 1.5f : lfoOut;
    const float releaseMs  = (lfoState.volRelLfoDepth < 1e-4f)
        ? lfoState.volRelBaseReleaseMs
        : juce::jmax (3.0f, lfoState.volRelBaseReleaseMs
                             * (1.0f + lfoState.volRelLfoDepth * boostedLfo));
    volumeEnv.setParameters (params.volAttackMs, params.volDecayMs,
                              params.volSustain, releaseMs);

    renderSamples (outputBuffer, startSample, numSamples, lfoState);
}
