#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "SuomiLookAndFeel.h"

// Transparent overlay drawn on top of a rotary slider.
// Paints a secondary "ghost" pointer showing the current LFO modulation position
// with the depth amount encoded in the sweep width.
// Visible only when the LFO depth parameter exceeds zero.
// Does not intercept mouse events — all clicks pass through to the slider beneath.
class LfoModIndicator : public juce::Component,
                        private juce::Timer
{
public:
    // FullLine  — full-radius pointer (standard knobs).
    // RimTip    — short tip at the outer rim only (semitoneKnob / centreTextKnob style).
    enum class Style { FullLine, RimTip };

    // Additive               — indicator = center + lfoOut × depth  (e.g. pitch LFO)
    // MultiplicativeBipolar  — indicator = base × (1 + depth × lfoOut)  (e.g. Vol Rel LFO)
    // ExponentialOctave      — indicator = center × 2^(lfoOut × depth × 4)  (e.g. filter cutoff, octave modulation)
    enum class ModType { Additive, MultiplicativeBipolar, ExponentialOctave };

    LfoModIndicator();

    // Point at the audio-thread atomics that drive the display.
    // lfoOutput: raw LFO signal in [-1, +1].
    // lfoDepth:  depth parameter; indicator is hidden when this is zero.
    void setSource (std::atomic<float>* lfoOutput, std::atomic<float>* lfoDepth);

    // Additive mode: optional knob center (e.g. semitoneOffset).
    void setCenterSource (std::atomic<float>* center) { centerSrc = center; }

    // MultiplicativeUnipolar mode: base value that the LFO scales (e.g. base release ms).
    void setBaseSource (std::atomic<float>* base) { baseSrc = base; }

    void setStyle        (Style          s) { pointerStyle  = s; }
    void setModType      (ModType        t) { modType       = t; }
    void setPointerColour (juce::Colour  c) { pointerColour = c; }

    // Amplification factor for negative LFO values in MultiplicativeBipolar mode.
    // Matches the asymmetric boost applied in the audio thread (e.g. 1.5× for vol release).
    void setNegativeBoost (float b) { negativeBoost = b; }

    // Optional second modulation source whose contribution is summed with the first.
    // Call this when Step and Env both target the same destination knob.
    void setSecondSource (std::atomic<float>* lfoOutput2, std::atomic<float>* lfoDepth2)
    {
        lfoOutputSrc2 = lfoOutput2;
        lfoDepthSrc2  = lfoDepth2;
    }

    // Linear destination range — used when the knob has a linear scale.
    void setDestinationRange (float rangeMin, float rangeMax);

    // Skewed / non-linear destination range — used when the knob has a NormalisableRange.
    // convertTo0to1() is used directly for the proportion.
    void setNormalisableRange (juce::NormalisableRange<float> range);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    std::atomic<float>* lfoOutputSrc  { nullptr };
    std::atomic<float>* lfoDepthSrc   { nullptr };
    std::atomic<float>* lfoOutputSrc2 { nullptr };
    std::atomic<float>* lfoDepthSrc2  { nullptr };
    std::atomic<float>* centerSrc     { nullptr };
    std::atomic<float>* baseSrc       { nullptr };

    Style         pointerStyle  { Style::FullLine };
    ModType       modType       { ModType::Additive };
    juce::Colour  pointerColour { SuomiLookAndFeel::kKnobPointer };

    float negativeBoost { 1.0f };

    bool  hasLinearRange { false };
    float destMin        { -1.0f };
    float destMax        {  1.0f };

    bool                          hasNormRange { false };
    juce::NormalisableRange<float> normRange;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LfoModIndicator)
};
