#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// YIN-based fundamental pitch detector for one-shot samples.
// All methods are blocking — run on a background thread, not the audio thread.
class PitchDetector
{
public:
    struct Result
    {
        float frequencyHz { 0.0f };
        int   midiNote    { -1 };     // -1 = detection failed
        float confidence  { 0.0f };   // 0–1 (higher = more confident)
    };

    // Detects the fundamental pitch of the given audio buffer.
    // startNorm / endNorm define the region to analyse (0–1, normalised
    // to the full buffer length).  Stereo buffers are mono-mixed internally.
    static Result detect (const juce::AudioBuffer<float>& buffer,
                          double sampleRate,
                          float startNorm = 0.0f,
                          float endNorm   = 1.0f);

private:
    struct YinResult { float period; float cmndfMin; };

    // Core YIN difference function + cumulative mean normalised difference.
    // Returns the estimated period in samples (fractional via parabolic interpolation)
    // and the CMNDF minimum value (lower = more confident).
    // Period is 0 if no clear pitch is found.
    static YinResult yinEstimatePeriod (const float* data, int numSamples,
                                        float threshold);
};
