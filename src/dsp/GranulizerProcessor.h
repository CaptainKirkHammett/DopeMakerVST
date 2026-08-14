#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <array>

// Granulizer: granular-synthesis post-processor inspired by FL Studio's Granulizer.
// Reads audio from the engine into a 2-second circular delay buffer and replays it
// as a stream of overlapping grains.  When disabled the input passes through unmodified
// but the delay buffer is still updated so re-enabling is glitch-free.
//
// Parameters (FL Studio–style semantics)
//   attackMs     — grain attack (fade-in) time in ms
//   holdMs       — grain hold (plateau) time in ms
//   spacingMs    — interval between successive grain starts in ms
//   waveSpacing  — -3..3, grain playback rate (stride through source waveform; negative = reverse)
//   panAmt       — 0..1, alternating L/R stereo spread (odd grains left, even right)
//   depth        — 0..1, amplitude of internal LFO that modulates waveSpacing
//   speed        — 0.01..20, frequency (Hz) of the wave-spacing LFO
//   randAmt      — 0..1, randomises grain start position and timing
class GranulizerProcessor
{
public:
    void prepare (double sampleRate, int maxBlockSize);

    void process (juce::AudioBuffer<float>& buffer,
                  bool  enabled,
                  float attackMs,
                  float holdMs,
                  float spacingMs,
                  float waveSpacing,
                  float panAmt,
                  float depth,
                  float speed,
                  float randAmt);

    // How far back (in seconds) the most recent active grain is reading relative
    // to the write head.  -1.0f when no grains are active or granulizer is off.
    // Written on the audio thread, read by the UI timer for the waveform marker.
    std::atomic<float> grainOffsetSeconds { -1.0f };

private:
    struct Grain
    {
        float readPos    { 0.0f };  // fractional absolute position in the circular delay buffer
        float panL       { 1.0f };
        float panR       { 1.0f };
        float playRate   { 1.0f };  // derived from waveSpacing + LFO at spawn time
        int   age        { 0    };
        int   attackSmp  { 1    };
        int   holdSmp    { 1    };
        int   releaseSmp { 1    };  // always equals attackSmp — symmetric fade-out
        int   totalSmp   { 3    };  // attackSmp + holdSmp + releaseSmp
        bool  active     { false };
    };

    static constexpr int kMaxGrains  = 16;
    static constexpr int kBufferSecs = 2;

    double sampleRate      { 44100.0 };
    int    delayBufferSize { 0 };
    int    writeHead       { 0 };

    juce::AudioBuffer<float> delayBuffer;   // circular, 2 s × 2 ch, pre-allocated in prepare()
    juce::AudioBuffer<float> scratchBuffer; // preserves input samples before the output is cleared

    std::array<Grain, kMaxGrains> grains {};
    float        schedulerTimer { 0.0f };
    float        lfoPhase       { 0.0f };  // internal wave-spacing LFO, [0, 2π)
    int          grainCounter   { 0   };   // for alternating L/R pan
    juce::Random rng;

    void  spawnGrain    (float effectiveWaveSpacing, float panAmt, float randAmt,
                         int attackSmp, int holdSmp);
    float grainEnvelope (const Grain& g) const;
    float readInterp    (int channel, float pos) const;
};
