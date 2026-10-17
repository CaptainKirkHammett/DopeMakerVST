#include "LfoModIndicator.h"
#include "SuomiLookAndFeel.h"

LfoModIndicator::LfoModIndicator()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
}

void LfoModIndicator::setSource (std::atomic<float>* lfoOutput, std::atomic<float>* lfoDepth)
{
    lfoOutputSrc = lfoOutput;
    lfoDepthSrc  = lfoDepth;
    startTimerHz (30);
}

void LfoModIndicator::setDestinationRange (float rangeMin, float rangeMax)
{
    destMin        = rangeMin;
    destMax        = rangeMax;
    hasLinearRange = true;
}

void LfoModIndicator::setNormalisableRange (juce::NormalisableRange<float> range)
{
    normRange    = range;
    hasNormRange = true;
}

void LfoModIndicator::paint (juce::Graphics& g)
{
    if (lfoOutputSrc == nullptr || lfoDepthSrc == nullptr)
        return;

    const float depth  = lfoDepthSrc->load  (std::memory_order_relaxed);
    const float lfoOut = lfoOutputSrc->load (std::memory_order_relaxed);

    float depth2  = 0.0f;
    float lfoOut2 = 0.0f;
    if (lfoOutputSrc2 != nullptr && lfoDepthSrc2 != nullptr)
    {
        depth2  = lfoDepthSrc2->load  (std::memory_order_relaxed);
        lfoOut2 = lfoOutputSrc2->load (std::memory_order_relaxed);
    }

    if (std::abs (depth) < 1e-4f && std::abs (depth2) < 1e-4f)
        return;

    const float w      = static_cast<float> (getWidth());
    const float h      = static_cast<float> (getHeight());
    const float radius = juce::jmin (w, h) * 0.5f - 4.0f;
    const float cx     = w * 0.5f;
    const float cy     = h * 0.5f;

    if (radius < 4.0f)
        return;

    // JUCE LookAndFeel_V4 default rotary arc: start = 1.2π (lower-left),
    // end = 2.8π (lower-right), sweeping clockwise through 12 o'clock.
    constexpr float kRotaryStart = juce::MathConstants<float>::pi * 1.2f;
    constexpr float kRotaryEnd   = juce::MathConstants<float>::pi * 2.8f;

    // Compute the raw modulated value from the appropriate formula, then convert
    // to a [0..1] proportion using whichever range has been configured.
    // Both sources contribute additively inside each formula so a single indicator
    // shows the combined effect when Step and Env target the same destination.
    const float contrib1 = lfoOut  * depth;
    const float contrib2 = lfoOut2 * depth2;

    float raw;
    if (modType == ModType::Additive)
    {
        const float center = (centerSrc != nullptr)
                                 ? centerSrc->load (std::memory_order_relaxed)
                                 : 0.0f;
        raw = center + contrib1 + contrib2;
    }
    else if (modType == ModType::MultiplicativeBipolar)  // base × (1 + sum of contributions)
    {
        const float base = (baseSrc != nullptr)
                               ? baseSrc->load (std::memory_order_relaxed)
                               : 1.0f;
        // Apply asymmetric negative boost to match the audio-thread formula
        // (e.g. vol release LFO uses 1.5× boost for negative swings).
        const float boosted1 = (lfoOut < 0.0f ? lfoOut * negativeBoost : lfoOut) * depth;
        raw = base * (1.0f + boosted1 + contrib2);
    }
    else  // ExponentialOctave: center × 2^((contrib1 + contrib2) × 4)  — e.g. filter cutoff in octaves
    {
        const float center = (centerSrc != nullptr)
                                 ? centerSrc->load (std::memory_order_relaxed)
                                 : 1.0f;
        raw = center * std::pow (2.0f, (contrib1 + contrib2) * 4.0f);
    }

    // Modulated proportion — where the knob would sit after modulation.
    float modulatedProportion;
    bool  clipping = false;
    if (hasNormRange)
    {
        if (modType != ModType::ExponentialOctave)
            clipping = (raw < normRange.start || raw > normRange.end);
        modulatedProportion = normRange.convertTo0to1 (juce::jlimit (normRange.start, normRange.end, raw));
    }
    else if (hasLinearRange)
    {
        clipping            = (raw < destMin || raw > destMax);
        modulatedProportion = (juce::jlimit (destMin, destMax, raw) - destMin) / (destMax - destMin);
    }
    else
    {
        modulatedProportion = (lfoOut + 1.0f) * 0.5f;
    }

    // Current (unmodulated) proportion — where the knob actually sits right now.
    // MultiplicativeBipolar tracks the base value (baseSrc); all other modes track centerSrc.
    const std::atomic<float>* currentSrc = (modType == ModType::MultiplicativeBipolar)
                                               ? baseSrc
                                               : centerSrc;
    float currentProportion = 0.5f;
    if (hasNormRange && currentSrc != nullptr)
    {
        const float cv = currentSrc->load (std::memory_order_relaxed);
        currentProportion = normRange.convertTo0to1 (juce::jlimit (normRange.start, normRange.end, cv));
    }
    else if (hasLinearRange && currentSrc != nullptr)
    {
        const float cv = currentSrc->load (std::memory_order_relaxed);
        currentProportion = (juce::jlimit (destMin, destMax, cv) - destMin) / (destMax - destMin);
    }
    else if (hasLinearRange)
    {
        // No center source: anchor the arc at the start of the track (kRotaryStart).
        // Indicators like the phaser/flanger LFO phase display pass no centerSrc —
        // sweeping from the track start makes the arc visibly grow and shrink as the
        // LFO oscillates, instead of oscillating around the invisible 12-o'clock centre.
        currentProportion = 0.0f;
    }

    // Convert to arc angles — JUCE addArc convention: clockwise from 12 o'clock, no halfPi offset.
    const float fromAngle = kRotaryStart + currentProportion   * (kRotaryEnd - kRotaryStart);
    const float toAngle   = kRotaryStart + modulatedProportion * (kRotaryEnd - kRotaryStart);

    const float arcStart = juce::jmin (fromAngle, toAngle);
    const float arcEnd   = juce::jmax (fromAngle, toAngle);

    if (arcEnd - arcStart < 0.005f)
        return;

    // Draw a tinted band on the outer rim of the knob between current and modulated positions.
    const juce::Colour arcCol   = clipping ? juce::Colour (0xffff8800) : pointerColour;
    // Arc sits right at the knob body edge, matching the rim ring drawn by the LookAndFeel.
    const float arcRadius = radius - 1.5f;

    juce::Path arc;
    arc.addArc (cx - arcRadius, cy - arcRadius,
                arcRadius * 2.0f, arcRadius * 2.0f,
                arcStart, arcEnd, true);

    const juce::PathStrokeType coreStroke  { 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded };
    const juce::PathStrokeType glowStroke  { 7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded };

    // Soft glow halo
    g.setColour (arcCol.withAlpha (0.30f));
    g.strokePath (arc, glowStroke);

    // Core rim band
    g.setColour (arcCol.withAlpha (0.88f));
    g.strokePath (arc, coreStroke);
}
