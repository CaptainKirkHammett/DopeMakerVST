#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

// Stereo vertical peak meter with PRE / POST label.
// Reads raw peak levels (linear amplitude) from a pair of atomics written by
// the audio thread.  All visual state lives on the UI thread — no locks needed.
class VuMeterComponent : public juce::Component, private juce::Timer
{
public:
    explicit VuMeterComponent (const juce::String& labelText);
    ~VuMeterComponent() override;

    // Point the meter at the processor's atomic level outputs.
    // Must be called before the component becomes visible.
    void setLevelSources (std::atomic<float>* leftLinear,
                          std::atomic<float>* rightLinear);

    // Optional: attach clip-FX visualization feeds from the processor.
    // When connected, the meter draws a clip-ceiling overlay on top of the bars.
    // preClipL/R   — block peak before the clip processor (linear amplitude).
    // threshold    — current clip ceiling (linear amplitude).
    // modeState    — ClipMode index (0=Hard, 1=Soft) or -1 when clip is off.
    void setClipSources (std::atomic<float>* preClipL,
                         std::atomic<float>* preClipR,
                         std::atomic<float>* threshold,
                         std::atomic<int>*   modeState);

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    // Exponential fall coefficient applied each timer tick (instant rise).
    static constexpr float kDecayCoeff    = 0.88f;
    // Ticks before the peak hold indicator starts falling (~1.5 s at 30 ms).
    static constexpr int   kPeakHoldTicks = 50;
    // Timer interval in milliseconds (~33 fps).
    static constexpr int   kTimerMs       = 30;
    // dB value that maps to an empty bar.
    static constexpr float kDbFloor       = -60.0f;

    const juce::String title;
    std::atomic<float>* sourceL { nullptr };
    std::atomic<float>* sourceR { nullptr };

    // All display state is owned exclusively by the UI thread.
    float displayL { 0.0f }, displayR { 0.0f };
    float peakL    { 0.0f }, peakR    { 0.0f };
    int   peakTicksL { 0 },  peakTicksR { 0 };

    juce::Rectangle<int> barAreaL, barAreaR, labelArea;

    // Clip overlay sources (all nullptr = no overlay drawn).
    std::atomic<float>* clipPrePeakSrcL  { nullptr };
    std::atomic<float>* clipPrePeakSrcR  { nullptr };
    std::atomic<float>* clipThresholdSrc { nullptr };
    std::atomic<int>*   clipModeSrc      { nullptr };

    // Decayed display values for the clip overlay — UI thread only.
    float displayClipPreL  { 0.0f };
    float displayClipPreR  { 0.0f };
    float displayClipThresh { 1.0f };
    int   displayClipMode  { -1   };

    void timerCallback() override;

    void drawBar (juce::Graphics&,
                  juce::Rectangle<int> area,
                  float displayLevel,
                  float peakLevel) const;

    // Draws the clip-ceiling overlay on top of a bar.
    // preClipPeak and threshold are linear amplitude; mode is 0=Hard, 1=Soft.
    void drawClipOverlay (juce::Graphics&,
                          juce::Rectangle<int> area,
                          float preClipPeak,
                          float threshold,
                          int   mode) const;

    // Maps linear amplitude [0..∞] to normalised bar fill [0..1] via a dB scale.
    static float linearToFill (float linear);
    // Returns the segment colour for a given fill position [0..1].
    static juce::Colour segmentColour (float fillPos);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VuMeterComponent)
};
