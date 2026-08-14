#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// Lock-free L/R sample FIFO for the goniometer visualiser.
// Audio thread calls pushSamples(); UI thread calls pullSamples() on a timer.
class StereoFieldSource
{
public:
    static constexpr int kFifoSize = 8192; // sample pairs

    StereoFieldSource() = default;

    // Audio thread: push a block of stereo samples.
    void pushSamples (const juce::AudioBuffer<float>& buffer)
    {
        const int nCh      = buffer.getNumChannels();
        const int nSamples = buffer.getNumSamples();
        if (nSamples == 0) return;

        const int toWrite = juce::jmin (nSamples, fifo.getFreeSpace());
        if (toWrite == 0) return;

        int s1, n1, s2, n2;
        fifo.prepareToWrite (toWrite, s1, n1, s2, n2);

        for (int i = 0; i < n1; ++i)
        {
            leftData [s1 + i] = nCh > 0 ? buffer.getSample (0, i)      : 0.0f;
            rightData[s1 + i] = nCh > 1 ? buffer.getSample (1, i)      : leftData[s1 + i];
        }
        for (int i = 0; i < n2; ++i)
        {
            leftData [s2 + i] = nCh > 0 ? buffer.getSample (0, n1 + i) : 0.0f;
            rightData[s2 + i] = nCh > 1 ? buffer.getSample (1, n1 + i) : leftData[s2 + i];
        }

        fifo.finishedWrite (n1 + n2);
    }

    // UI thread: pull up to maxCount pairs into caller-supplied arrays.
    // Returns the number of pairs actually written.
    int pullSamples (float* left, float* right, int maxCount)
    {
        const int avail = juce::jmin (fifo.getNumReady(), maxCount);
        if (avail == 0) return 0;

        int s1, n1, s2, n2;
        fifo.prepareToRead (avail, s1, n1, s2, n2);

        for (int i = 0; i < n1; ++i) { left[i]      = leftData[s1 + i]; right[i]      = rightData[s1 + i]; }
        for (int i = 0; i < n2; ++i) { left[n1 + i] = leftData[s2 + i]; right[n1 + i] = rightData[s2 + i]; }

        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

private:
    juce::AbstractFifo fifo { kFifoSize };
    std::array<float, kFifoSize> leftData  {};
    std::array<float, kFifoSize> rightData {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoFieldSource)
};
