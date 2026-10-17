#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

// Mid-Side stereo width processor with a single "width" control (-1..+1).
//
//  -1.0  →  fully mono   (side = 0)
//   0.0  →  unchanged    (side = 1× original)
//  +1.0  →  maximum wide (side = 2× + Haas-based artificial stereo)
//
// Artificial widening for mono/narrow sources uses a short delay (~7 ms)
// applied to the mid signal.  The delayed mid is used as an artificial side
// component (added to L, subtracted from R).  Because the autocorrelation of
// broadband audio at 7 ms is very low, L and R receive nearly equal energy —
// no consistent channel-level imbalance.  7 ms is well below the Haas fusion
// threshold (~30 ms), so it is heard as width rather than an echo.
class StereoProcessor
{
public:
    StereoProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void process (juce::dsp::AudioBlock<float>& block);
    void reset();

    // width: -1.0 = mono, 0.0 = unchanged, +1.0 = widest. Call once per block.
    void setWidth (float width);

private:
    static constexpr int kMaxDelay = 2048;   // covers up to ~43 ms at 48 kHz
    std::array<float, kMaxDelay> midDelay {};
    int delayWritePtr { 0 };
    int delaySamples  { 0 };                 // set from sampleRate in prepare()

    juce::SmoothedValue<float> widthSmoothed;
};
