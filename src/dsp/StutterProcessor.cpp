#include "StutterProcessor.h"
#include <cmath>

const juce::StringArray StutterProcessor::kDivisionNames {
    "1/4", "1/8", "1/16", "1/32", "1/64"
};

// Division lengths in quarter-note beats.
static constexpr double kDivBeats[StutterProcessor::kNumDivisions] = {
    1.0, 0.5, 0.25, 0.125, 0.0625
};

void StutterProcessor::prepare (double sampleRate, int /*maxBlockSize*/)
{
    sampleRate_       = sampleRate;
    lfoPhase_         = 0.0;
    lfoRandCurrent_   = rng_.nextFloat();
    lfoRandNext_      = rng_.nextFloat();
    stutterActive_    = false;
    stutterReadPos_   = 0;
    captureLength_    = 0;
    captureWritePos_  = 0;
    fadeOutRemaining_  = 0;
    fadeInRemaining_   = 0;
    fadeInTotal_       = 0;
    loopFadeRemaining_ = 0;
    loopFadeOldL_      = 0.0f;
    loopFadeOldR_      = 0.0f;
    lastPpq_           = -1.0;
    lastDivIdx_       = -1;
    consecutiveFast_   = 0;
    prevDivIdx_        = -1;
    activeDivIdx_      = -1;
    minStepsRemaining_ = 0;

    // Smooth play-length changes over ~3 ms to avoid clicks from LFO modulation.
    smoothedPlayLen_.reset (sampleRate, 0.003);

    const int bufSize = static_cast<int> (kMaxBufferSecs * sampleRate);
    captureBuffer.setSize (2, bufSize, false, true, false);
}

double StutterProcessor::divisionToBeats (int divIdx) const
{
    return kDivBeats[juce::jlimit (0, kNumDivisions - 1, divIdx)];
}

int StutterProcessor::effectiveDivIndex (float lfoLow, float lfoHigh) const
{
    const float lo = juce::jlimit (0.0f, float (kNumDivisions - 1), lfoLow);
    const float hi = juce::jlimit (0.0f, float (kNumDivisions - 1), lfoHigh);

    // Random sine: cosine-interpolate between two random values for smooth randomness.
    const float t = 0.5f - 0.5f * static_cast<float> (std::cos (
        juce::MathConstants<double>::pi * lfoPhase_));
    const float lfoVal = lfoRandCurrent_ + t * (lfoRandNext_ - lfoRandCurrent_);

    return juce::roundToInt (lo + lfoVal * (hi - lo));
}

int StutterProcessor::constrainFastDiv (int candidate, float lfoLow, float lfoHigh)
{
    const bool isFast = (candidate >= 3);  // 3=1/32, 4=1/64

    // When the user locks both range ends to the same division, honour it
    // unconditionally — the constraint only makes sense for randomised ranges.
    if (juce::roundToInt (lfoLow) == juce::roundToInt (lfoHigh))
    {
        consecutiveFast_ = 0;
        prevDivIdx_ = candidate;
        return candidate;
    }

    if (!isFast)
    {
        // Slow division — reset the fast counter.
        consecutiveFast_ = 0;
        prevDivIdx_ = candidate;
        return candidate;
    }

    // Fast division requested.
    if (consecutiveFast_ >= 2)
    {
        // Already had two consecutive fast steps — force a slow division.
        // Pick randomly from slow divisions (0=1/4, 1=1/8, 2=1/16) within the
        // selected [lfoLow, lfoHigh] range.
        const int lo = juce::jlimit (0, 2, static_cast<int> (lfoLow));
        const int hi = juce::jlimit (0, 2, static_cast<int> (lfoHigh));
        const int slowIdx = lo + rng_.nextInt (juce::jmax (1, hi - lo + 1));

        consecutiveFast_ = 0;
        prevDivIdx_ = slowIdx;
        return slowIdx;
    }

    if (consecutiveFast_ == 1 && candidate == prevDivIdx_)
    {
        // Same fast division twice in a row — not allowed.
        // Try the other fast division if it's in range.
        const int otherFast = (candidate == 3) ? 4 : 3;
        const int maxIdx = juce::jlimit (0, kNumDivisions - 1, static_cast<int> (juce::jmax (lfoLow, lfoHigh)));
        if (otherFast <= maxIdx)
        {
            consecutiveFast_ = 2;
            prevDivIdx_ = otherFast;
            return otherFast;
        }
        // Other fast division not in range — fall back to slow.
        const int lo = juce::jlimit (0, 2, static_cast<int> (lfoLow));
        const int hi = juce::jlimit (0, 2, static_cast<int> (lfoHigh));
        const int slowIdx = lo + rng_.nextInt (juce::jmax (1, hi - lo + 1));

        consecutiveFast_ = 0;
        prevDivIdx_ = slowIdx;
        return slowIdx;
    }

    // First fast step, or second fast step with a different division — allow it.
    consecutiveFast_++;
    prevDivIdx_ = candidate;
    return candidate;
}

void StutterProcessor::process (juce::AudioBuffer<float>& buffer, int numSamples,
                                double bpm, double ppqPosition, bool enabled,
                                float amount, float lfoSpeed,
                                float lfoLow, float lfoHigh, float length,
                                bool repeatMode)
{
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int bufCapacity = captureBuffer.getNumSamples();

    // Advance LFO even when disabled so it doesn't snap on enable.
    const double lfoInc = static_cast<double> (lfoSpeed) / sampleRate_;
    const double newPhase = lfoPhase_ + lfoInc * numSamples;
    if (newPhase >= 1.0)
    {
        lfoRandCurrent_ = lfoRandNext_;
        lfoRandNext_    = rng_.nextFloat();
    }
    lfoPhase_ = std::fmod (newPhase, 1.0);

    // Advance LFO to get a candidate division, but do NOT use it for boundary
    // detection.  Boundaries are always based on the committed activeDivIdx_.
    const int rawIdx = effectiveDivIndex (lfoLow, lfoHigh);

    if (!enabled)
    {
        // If stutter was playing, crossfade out instead of cutting instantly.
        if (stutterActive_)
            fadeOutRemaining_ = kCrossfadeSamples;

        stutterActive_    = false;
        fadeInRemaining_  = 0;
        consecutiveFast_   = 0;
        prevDivIdx_        = -1;
        lastPpq_           = -1.0;
        lastDivIdx_        = -1;
        activeDivIdx_      = -1;
        minStepsRemaining_ = 0;

        // Keep filling capture buffer so enabling is glitch-free.
        // Process fade-out if one is active (stutter was playing when disabled).
        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                captureBuffer.setSample (ch, captureWritePos_, buffer.getSample (ch, s));
            captureWritePos_ = (captureWritePos_ + 1) % bufCapacity;
            captureLength_ = juce::jmin (captureLength_ + 1, bufCapacity);

            if (fadeOutRemaining_ > 0)
            {
                const float dryL = (numChannels > 0) ? buffer.getSample (0, s) : 0.0f;
                const float dryR = (numChannels > 1) ? buffer.getSample (1, s) : 0.0f;
                const float t = static_cast<float> (fadeOutRemaining_) / static_cast<float> (kCrossfadeSamples);

                if (numChannels > 0) buffer.getWritePointer (0)[s] = lastWetL_ * t + dryL * (1.0f - t);
                if (numChannels > 1) buffer.getWritePointer (1)[s] = lastWetR_ * t + dryR * (1.0f - t);

                const float decay = static_cast<float> (fadeOutRemaining_ - 1) / static_cast<float> (fadeOutRemaining_);
                lastWetL_ *= decay;
                lastWetR_ *= decay;
                --fadeOutRemaining_;
            }
        }
        currentEffectiveDivIndex.store (rawIdx, std::memory_order_relaxed);
        return;
    }

    const double safeBpm = bpm > 0.0 ? bpm : 120.0;

    // On first enable, commit the initial division so boundary detection works.
    if (activeDivIdx_ < 0)
        activeDivIdx_ = constrainFastDiv (rawIdx, lfoLow, lfoHigh);

    currentEffectiveDivIndex.store (activeDivIdx_, std::memory_order_relaxed);

    // Use the committed division for boundary detection — this keeps timing stable.
    const double divBeats = divisionToBeats (activeDivIdx_);

    // Determine if a boundary crossing happened this block using PPQ position.
    bool boundaryThisBlock = false;
    int  boundarySampleOffset = 0;

    if (ppqPosition >= 0.0)
    {
        const double ppqEnd = ppqPosition + (numSamples / sampleRate_) * (safeBpm / 60.0);

        const double startBeat = std::floor (ppqPosition / divBeats);
        const double endBeat   = std::floor (ppqEnd / divBeats);

        if (endBeat > startBeat || lastPpq_ < 0.0)
        {
            boundaryThisBlock = true;
            const double nextBoundaryPpq = (startBeat + 1.0) * divBeats;
            if (nextBoundaryPpq <= ppqPosition)
            {
                boundarySampleOffset = 0;
            }
            else
            {
                const double ppqUntilBoundary = nextBoundaryPpq - ppqPosition;
                boundarySampleOffset = juce::jlimit (0, numSamples - 1,
                    static_cast<int> ((ppqUntilBoundary / (safeBpm / 60.0)) * sampleRate_));
            }
        }

        lastPpq_ = ppqEnd;
    }
    else
    {
        lastPpq_ = -1.0;
        if (stutterActive_)
        {
            stutterActive_    = false;
            fadeOutRemaining_ = kCrossfadeSamples;
        }
    }

    // Continuously update play length target from the (possibly LFO-modulated) length.
    // This lets the Len LFO modulate smoothly during active playback.
    if (stutterActive_ && stutterSegLen_ > 0)
    {
        const float clampedLen = juce::jlimit (0.0f, 1.0f, length);
        const int targetPlayLen = juce::jmax (1, static_cast<int> (clampedLen * stutterSegLen_));
        smoothedPlayLen_.setTargetValue (static_cast<float> (targetPlayLen));
    }

    // Helper: read a sample from the frozen stutter segment.
    auto readSeg = [&] (int ch, int pos) -> float
    {
        if (stutterSegLen_ <= 0) return 0.0f;
        const int readIdx = (stutterSegStart_ + (pos % stutterSegLen_)) % bufCapacity;
        return captureBuffer.getSample (ch, readIdx);
    };

    for (int s = 0; s < numSamples; ++s)
    {
        // Check for boundary at this sample.
        if (boundaryThisBlock && s == boundarySampleOffset)
        {
            boundaryThisBlock = false;  // consume the event

            // During minimum gate, lock the division so fast repeats stay consistent.
            if (minStepsRemaining_ <= 0)
            {
                const int prevDiv = activeDivIdx_;
                activeDivIdx_ = constrainFastDiv (rawIdx, lfoLow, lfoHigh);
                currentEffectiveDivIndex.store (activeDivIdx_, std::memory_order_relaxed);

                // If we just transitioned to a fast division (even mid-stutter),
                // start a minimum gate so it plays for a full 1/8 burst.
                if (activeDivIdx_ >= 3 && prevDiv < 3 && stutterActive_)
                {
                    minStepsRemaining_ = (activeDivIdx_ == 4) ? kMinSteps64 : kMinSteps32;
                }
            }

            // Compute interval for the committed division.
            const double newBeats    = divisionToBeats (activeDivIdx_);
            const double newSamples  = (newBeats / (safeBpm / 60.0)) * sampleRate_;
            const int    newInterval = juce::jmax (1, static_cast<int> (newSamples));

            if (!stutterActive_)
            {
                // Currently OFF — probability chance to turn ON.
                if (rng_.nextFloat() < amount)
                {
                    stutterActive_   = true;
                    stutterReadPos_  = 0;
                    stutterSegLen_   = juce::jmin (newInterval, captureLength_);
                    stutterSegStart_ = (captureWritePos_ - stutterSegLen_ + bufCapacity) % bufCapacity;
                    lengthParam_     = juce::jlimit (0.0f, 1.0f, length);
                    stutterPlayLen_  = juce::jmax (1, static_cast<int> (lengthParam_ * stutterSegLen_));
                    smoothedPlayLen_.setCurrentAndTargetValue (static_cast<float> (stutterPlayLen_));
                    lenWrapFading_      = false;
                    loopFadeRemaining_  = 0;
                    fadeInTotal_        = juce::jmin (kCrossfadeSamples, stutterPlayLen_ / 2);
                    fadeInRemaining_    = fadeInTotal_;

                    // Fast divisions get a minimum gate so they produce
                    // a usable burst of rapid repeats instead of a single blip.
                    // Both 1/32 and 1/64 play for a full 1/8 note.
                    if (activeDivIdx_ == 4)
                        minStepsRemaining_ = kMinSteps64;
                    else if (activeDivIdx_ == 3)
                        minStepsRemaining_ = kMinSteps32;
                    else
                        minStepsRemaining_ = 0;
                }
            }
            else
            {
                // Decrement minimum-gate counter for fast divisions.
                if (minStepsRemaining_ > 0)
                    --minStepsRemaining_;

                if (minStepsRemaining_ > 0)
                {
                    // Still inside minimum gate — keep looping the frozen segment.
                }
                else if (rng_.nextFloat() >= amount)
                {
                    // Minimum gate expired — probability chance to turn OFF.
                    // amount=1 → never turns off (100% stutter); amount=0 → always
                    // turns off.  Using (1-amount) keeps the knob intuitive: higher
                    // probability = more stutter.
                    fadeOutRemaining_ = kCrossfadeSamples;
                    stutterActive_ = false;
                }
                // else: stay on — keep looping the same frozen segment (no recapture)
            }
        }

        // Compute dry sample.
        float dryL = (numChannels > 0) ? buffer.getSample (0, s) : 0.0f;
        float dryR = (numChannels > 1) ? buffer.getSample (1, s) : 0.0f;

        // Only write to capture buffer when stutter is NOT active.
        // This prevents the write head from overwriting the frozen segment.
        if (!stutterActive_)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                captureBuffer.setSample (ch, captureWritePos_, buffer.getSample (ch, s));
            captureWritePos_ = (captureWritePos_ + 1) % bufCapacity;
            captureLength_ = juce::jmin (captureLength_ + 1, bufCapacity);
        }

        if (stutterActive_)
        {
            // Smoothly update play length per-sample from LFO-modulated target.
            stutterPlayLen_ = juce::jmax (1, static_cast<int> (smoothedPlayLen_.getNextValue()));

            // In repeat mode, if read pos exceeds the (now shorter) play length,
            // wrap back and crossfade to avoid a click.
            if (repeatMode && stutterReadPos_ >= stutterPlayLen_ && stutterSegLen_ > 0)
            {
                lenWrapOldL_ = lastWetL_;
                lenWrapOldR_ = lastWetR_;
                lenWrapFading_ = true;
                lenWrapFadeTotal_  = juce::jmax (1, juce::jmin (kCrossfadeSamples, stutterPlayLen_ / 2));
                lenWrapFadeRemain_ = lenWrapFadeTotal_;
                stutterReadPos_ = stutterReadPos_ % stutterPlayLen_;
            }

            float wetL = 0.0f, wetR = 0.0f;

            const int xfade = juce::jmax (1, juce::jmin (kCrossfadeSamples, stutterPlayLen_ / 2));

            if (stutterReadPos_ < stutterPlayLen_ && stutterSegLen_ > 0)
            {
                wetL = readSeg (0, stutterReadPos_);
                wetR = (numChannels > 1) ? readSeg (1, stutterReadPos_) : wetL;

                // In gate mode, fade out near the end of the play region so
                // the transition to the silent portion is smooth.
                if (!repeatMode)
                {
                    const int distToEnd = stutterPlayLen_ - stutterReadPos_;
                    if (distToEnd <= xfade)
                    {
                        const float t = static_cast<float> (distToEnd) / static_cast<float> (xfade);
                        wetL *= t;
                        wetR *= t;
                    }
                }
            }
            // else: beyond play length — wet is silence (0)

            // Crossfade at loop-wrap point — blends the tail of the previous
            // loop iteration into the head of the new one, maintaining signal
            // level instead of dipping to zero (eliminates clicks at fast rates).
            if (loopFadeRemaining_ > 0)
            {
                const float t = static_cast<float> (loopFadeRemaining_) / static_cast<float> (xfade);
                wetL = loopFadeOldL_ * t + wetL * (1.0f - t);
                wetR = loopFadeOldR_ * t + wetR * (1.0f - t);
                const float decay = static_cast<float> (loopFadeRemaining_ - 1)
                                  / static_cast<float> (loopFadeRemaining_);
                loopFadeOldL_ *= decay;
                loopFadeOldR_ *= decay;
                --loopFadeRemaining_;
            }

            // Crossfade when play length shrunk and read pos wrapped.
            if (lenWrapFading_ && lenWrapFadeRemain_ > 0)
            {
                const float t = static_cast<float> (lenWrapFadeRemain_) / static_cast<float> (lenWrapFadeTotal_);
                wetL = lenWrapOldL_ * t + wetL * (1.0f - t);
                wetR = lenWrapOldR_ * t + wetR * (1.0f - t);
                const float decay = static_cast<float> (lenWrapFadeRemain_ - 1)
                                  / static_cast<float> (lenWrapFadeRemain_);
                lenWrapOldL_ *= decay;
                lenWrapOldR_ *= decay;
                --lenWrapFadeRemain_;
                if (lenWrapFadeRemain_ <= 0)
                    lenWrapFading_ = false;
            }

            // Crossfade from dry to wet at stutter activation.
            if (fadeInRemaining_ > 0)
            {
                const int denom = juce::jmax (1, fadeInTotal_);
                const float t = 1.0f - static_cast<float> (fadeInRemaining_) / static_cast<float> (denom);
                wetL = dryL * (1.0f - t) + wetL * t;
                wetR = dryR * (1.0f - t) + wetR * t;
                --fadeInRemaining_;
            }

            // Save last output for potential fade-out.
            lastWetL_ = wetL;
            lastWetR_ = wetR;

            if (numChannels > 0) buffer.getWritePointer (0)[s] = wetL;
            if (numChannels > 1) buffer.getWritePointer (1)[s] = wetR;

            stutterReadPos_++;
            // Gate mode: loop at the full segment length — samples beyond
            // stutterPlayLen_ are silent, controlled by the Len knob.
            // Repeat mode: loop at stutterPlayLen_ directly — shorter Len
            // means a shorter loop and faster repetition, no silent gap.
            const int loopLen = repeatMode ? stutterPlayLen_ : stutterSegLen_;
            if (stutterReadPos_ >= loopLen)
            {
                stutterReadPos_ = 0;

                // Trigger a crossfade from the tail of the finished loop into the
                // head of the new one.  lastWetL_/R_ holds the processed output
                // of the sample just before the wrap.
                loopFadeOldL_      = lastWetL_;
                loopFadeOldR_      = lastWetR_;
                loopFadeRemaining_ = xfade;
            }
        }
        else if (fadeOutRemaining_ > 0)
        {
            // Crossfade from last wet output back to dry.
            const float t = static_cast<float> (fadeOutRemaining_) / static_cast<float> (kCrossfadeSamples);

            if (numChannels > 0) buffer.getWritePointer (0)[s] = lastWetL_ * t + dryL * (1.0f - t);
            if (numChannels > 1) buffer.getWritePointer (1)[s] = lastWetR_ * t + dryR * (1.0f - t);

            // Decay the saved wet sample towards zero so the tail fades smoothly.
            const float decay = static_cast<float> (fadeOutRemaining_ - 1) / static_cast<float> (fadeOutRemaining_);
            lastWetL_ *= decay;
            lastWetR_ *= decay;

            --fadeOutRemaining_;
        }
        // else: pass-through (buffer already contains dry signal)
    }
}
