#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// Thread-safe sample pipeline for spectrum visualisation.
//
// Audio thread calls pushSamples() every processBlock().
// UI thread calls pullSpectrum() on a timer; returns true when a new FFT
// frame is ready and fills the caller's magnitude array.
//
// Thread safety: juce::AbstractFifo handles the producer/consumer boundary.
// FFT and smoothing run entirely on the UI thread — no shared mutable state.
class SpectrumSource
{
public:
    static constexpr int kFftOrder = 11;           // 2048-point FFT
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kNumBins  = kFftSize / 2;
    static constexpr int kFifoSize = kFftSize * 4;

    SpectrumSource()
        : fft    (kFftOrder),
          window (kFftSize, juce::dsp::WindowingFunction<float>::hann, true)
    {
        fifoData.fill (0.0f);
        fftBuffer.fill (0.0f);
        accumulator.fill (0.0f);
        smoothed.fill (0.0f);
    }

    // Called from prepareToPlay — safe to call from any thread before streaming starts.
    void setSampleRate (double sr) { sampleRate.store (sr); }
    double getSampleRate() const   { return sampleRate.load(); }

    // Audio thread: mono-mix the buffer and push into the FIFO.
    // Drops samples silently if the FIFO is full (UI thread too slow).
    void pushSamples (const juce::AudioBuffer<float>& buffer)
    {
        const int nCh      = buffer.getNumChannels();
        const int nSamples = buffer.getNumSamples();
        if (nCh == 0 || nSamples == 0)
            return;

        const int toWrite = juce::jmin (nSamples, fifo.getFreeSpace());
        if (toWrite == 0)
            return;

        int s1, n1, s2, n2;
        fifo.prepareToWrite (toWrite, s1, n1, s2, n2);

        auto writeMono = [&] (int srcIdx, int dstIdx)
        {
            float mono = 0.0f;
            for (int ch = 0; ch < nCh; ++ch)
                mono += buffer.getSample (ch, srcIdx);
            fifoData[dstIdx] = mono / (float) nCh;
        };

        for (int i = 0; i < n1; ++i) writeMono (i,      s1 + i);
        for (int i = 0; i < n2; ++i) writeMono (n1 + i, s2 + i);

        fifo.finishedWrite (n1 + n2);
    }

    // UI thread: drain the FIFO, accumulate samples, run FFT when a full frame
    // is ready, and apply smoothing. Returns true and updates magnitudes[] when
    // a new frame is available. Magnitudes are in [0..1] (log-normalised dB).
    bool pullSpectrum (std::array<float, kNumBins>& magnitudes)
    {
        bool newFrame = false;

        const int avail = fifo.getNumReady();
        if (avail > 0)
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead (avail, s1, n1, s2, n2);

            auto consume = [&] (float sample)
            {
                accumulator[accumIndex++] = sample;
                if (accumIndex == kFftSize)
                {
                    runFft();
                    accumIndex = 0;
                    newFrame = true;
                }
            };

            for (int i = 0; i < n1; ++i) consume (fifoData[s1 + i]);
            for (int i = 0; i < n2; ++i) consume (fifoData[s2 + i]);

            fifo.finishedRead (n1 + n2);
        }

        if (newFrame)
            magnitudes = smoothed;

        return newFrame;
    }

private:
    void runFft()
    {
        // Copy real samples into the first half; zero the imaginary half.
        std::copy (accumulator.begin(), accumulator.end(), fftBuffer.begin());
        std::fill (fftBuffer.begin() + kFftSize, fftBuffer.end(), 0.0f);

        window.multiplyWithWindowingTable (fftBuffer.data(), kFftSize);
        fft.performFrequencyOnlyForwardTransform (fftBuffer.data());

        for (int i = 0; i < kNumBins; ++i)
        {
            const float mag  = fftBuffer[i] / (float) kFftSize;
            const float db   = juce::Decibels::gainToDecibels (mag, -100.0f);
            // Map −100 dB → 0.0,  0 dB → 1.0
            const float norm = juce::jlimit (0.0f, 1.0f, (db + 100.0f) / 100.0f);

            // Fast attack, slow decay for visual persistence.
            smoothed[i] = norm > smoothed[i]
                ? norm * 0.6f + smoothed[i] * 0.4f
                : norm * 0.05f + smoothed[i] * 0.95f;
        }
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::atomic<double> sampleRate { 44100.0 };

    juce::AbstractFifo fifo { kFifoSize };
    std::array<float, kFifoSize>    fifoData    {};
    std::array<float, kFftSize * 2> fftBuffer   {};
    std::array<float, kFftSize>     accumulator {};
    std::array<float, kNumBins>     smoothed    {};
    int accumIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumSource)
};
