#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>

// Stutter: audio-based beat-repeat effect.
// Captures incoming audio into a circular buffer, then at tempo-synced intervals
// replays a frozen segment.  Placed just before the filter in the FX chain.
// Trigger timing is synced to DAW transport via PPQ position.
class StutterProcessor
{
public:
    static constexpr int kNumDivisions = 5;

    // Division names in display order (index 0 = 1/4 … index 4 = 1/64).
    static const juce::StringArray kDivisionNames;

    void prepare (double sampleRate, int maxBlockSize);

    // Process audio in-place.  When a stutter fires, the output is replaced with
    // the repeated captured segment.  Crossfades are applied at boundaries to avoid clicks.
    // ppqPosition : current PPQ from DAW playhead (-1 if not playing).
    // bpm         : current tempo.
    // amount      : 0-1 probability of a stutter firing at each interval boundary.
    // lfoLow/High : effective division index range [0, kNumDivisions-1].
    // lfoSpeed    : LFO rate in Hz for modulating between lfoLow and lfoHigh.
    // length      : 0-1 fraction of the stutter interval that is replayed (rest is silent).
    void process (juce::AudioBuffer<float>& buffer, int numSamples,
                  double bpm, double ppqPosition, bool enabled, float amount,
                  float lfoSpeed, float lfoLow, float lfoHigh,
                  float length, bool repeatMode);

    // Effective division index for the current block — written here, read by UI timer.
    std::atomic<int> currentEffectiveDivIndex { 2 };  // default: 1/16

private:
    static constexpr int kCrossfadeSamples    = 512;  // ~11.6 ms at 44.1k — smooth transitions
    static constexpr int kMinCrossfadeSamples = 32;   // ~0.7 ms — floor to prevent clicks at short play lengths
    static constexpr int kMinPlayLenSamples   = 64;   // play length can never shrink below this
    static constexpr int kMaxBufferSecs    = 2;

    double sampleRate_ = 44100.0;
    double lfoPhase_   = 0.0;    // [0, 1)

    // Random-sine LFO: cosine-interpolate between successive random values.
    float lfoRandCurrent_ = 0.5f;  // random value at start of current cycle
    float lfoRandNext_    = 0.5f;  // random value at end of current cycle

    // Capture buffer: holds up to 2 seconds of audio for replaying.
    juce::AudioBuffer<float> captureBuffer;
    int captureLength_   = 0;    // number of valid samples in the capture buffer
    int captureWritePos_ = 0;    // write position in capture buffer

    // Playback state when a stutter is active.
    bool  stutterActive_   = false;
    int   stutterReadPos_  = 0;   // current read position within the captured segment
    int   stutterSegStart_ = 0;   // frozen start index in circular buffer when stutter began
    int   stutterSegLen_   = 0;   // total captured segment length (= interval in samples)
    int   maxSegLen_       = 0;   // segment length at activation — cap to avoid reading stale buffer audio
    int   stutterPlayLen_  = 0;   // how many samples to replay before going silent (length * segLen)

    // Crossfade state.
    int   fadeOutRemaining_ = 0;  // samples remaining in fade-out (stutter→dry transition)
    int   fadeInRemaining_  = 0;  // samples remaining in fade-in at stutter start
    int   fadeInTotal_      = 0;  // initial fadeInRemaining_ value (used as denominator)

    // Loop-wrap crossfade.  Two modes:
    //  • Continuation (repeat mode, playLen < segLen): reads forward from the
    //    capture buffer past the play length, giving a real waveform crossfade.
    //  • Single-sample (gate mode or when continuation is unavailable): blends
    //    from the frozen last output sample — simpler, always continuous.
    int   loopFadeRemaining_ = 0;
    int   loopFadeTotal_     = 0;     // denominator — fixed at trigger time
    bool  loopFadeContinuation_ = false;  // true = read from capture buffer
    int   loopFadeReadPos_   = 0;     // continuation read position
    float loopFadeOldL_      = 0.0f;  // single-sample fallback
    float loopFadeOldR_      = 0.0f;

    // Saved last wet sample for fade-out after stutter deactivates.
    float lastWetL_ = 0.0f;
    float lastWetR_ = 0.0f;

    float lengthParam_ = 1.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedPlayLen_;
    bool  lenWrapFading_     = false;  // crossfade active after read pos forced back
    int   lenWrapFadeRemain_ = 0;
    int   lenWrapFadeTotal_  = 0;     // denominator — fixed at trigger time
    float lenWrapOldL_       = 0.0f;  // saved sample at old read pos for crossfade
    float lenWrapOldR_       = 0.0f;

    // PPQ-based sync.
    double lastPpq_      = -1.0;
    int    lastDivIdx_   = -1;
    int    activeDivIdx_ = -1;  // committed division — only changes at boundaries

    // Fast-division constraint: indices 3 (1/32) and 4 (1/64) can only play
    // for one step each.  After one consecutive fast burst, a slow division
    // (1/4, 1/8, or 1/16) is forced.
    int  consecutiveFast_ = 0;   // how many consecutive fast steps so far
    int  prevDivIdx_      = -1;  // division used on the previous step

    // Minimum hold: every division (slow or fast) is locked for at least 1/16 note
    // in samples.  Aligns division changes to a 1/16 grid — the common divisor
    // of all divisions — so fast→slow transitions don't land mid-cycle.
    int  holdSamplesRemaining_ = 0;

    // Computes the hold duration in samples for a 1/16 note at the given tempo.
    int  holdSamplesFor16th (double bpm) const;

    juce::Random rng_;

    double divisionToBeats (int divIdx) const;
    int    effectiveDivIndex (float lfoLow, float lfoHigh) const;
    // Apply fast-division constraint to a candidate index. May override it.
    int    constrainFastDiv (int candidate, float lfoLow, float lfoHigh);
};
