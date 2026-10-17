#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

// Compact vertical bar meter showing the effective velocity of the last played note.
// Jumps instantly to the new velocity, holds briefly, then decays to zero.
// Written to by the audio thread via engine atomics; safe to read from the timer.
class VelocityMeterComponent : public juce::Component,
                               private juce::Timer
{
public:
    VelocityMeterComponent();

    // Point at the two engine atomics.  Starts the repaint timer.
    void setSource (std::atomic<float>*    lastVelocity,
                    std::atomic<uint32_t>* generation);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::atomic<float>*    velocitySrc   { nullptr };
    std::atomic<uint32_t>* generationSrc { nullptr };

    uint32_t lastGeneration { 0 };
    float    displayLevel   { 0.0f };
    int      holdFrames     { 0 };

    static constexpr float    kDecayPerFrame { 1.0f / (30.0f * 0.12f) }; // ~120 ms full decay
    static constexpr int      kHoldFrames    { 2 };                        // ~66 ms hold

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VelocityMeterComponent)
};
