#pragma once

#include <juce_dsp/juce_dsp.h>

class ReverbProcessor
{
public:
    ReverbProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::ProcessContextReplacing<float>& context);
    void reset();

    void setParameters (float roomSize, float damping, float width, float mix,
                        bool reverse, float reverseLengthMs);

private:
    juce::dsp::Reverb reverb;
    juce::dsp::ProcessSpec lastSpec { 44100.0, 512, 2 };

    float currentRoomSize { 0.5f };
    float currentDamping  { 0.5f };
    float currentWidth    { 1.0f };
    float currentMix      { 0.3f };

    // --- Reverse reverb state ---
    bool  reverseEnabled      { false };
    int   reverseLengthSamps  { 0 };

    // Two alternating segments: while one plays back, the other captures.
    struct Segment
    {
        juce::AudioBuffer<float> captureBuffer;
        juce::AudioBuffer<float> outputBuffer;
        juce::dsp::Reverb        reverb;
        int  capturePos   { 0 };
        int  playbackPos  { 0 };
        int  segmentLen   { 0 };
        bool outputReady  { false };
    };

    Segment segments[2];
    int activeCapture { 0 };   // index of the segment currently capturing

    static constexpr int kFadeLen = 64;   // ~1.5 ms at 44.1 kHz — just enough to avoid clicks

    void processSegment (Segment& seg);
};
