#include "VelocityMeterComponent.h"
#include "SuomiLookAndFeel.h"

VelocityMeterComponent::VelocityMeterComponent()
{
    setOpaque (false);
}

void VelocityMeterComponent::setSource (std::atomic<float>*    lastVelocity,
                                        std::atomic<uint32_t>* generation)
{
    velocitySrc   = lastVelocity;
    generationSrc = generation;
    startTimerHz (30);
}

void VelocityMeterComponent::timerCallback()
{
    if (velocitySrc == nullptr || generationSrc == nullptr)
        return;

    const uint32_t gen = generationSrc->load (std::memory_order_relaxed);
    if (gen != lastGeneration)
    {
        lastGeneration = gen;
        displayLevel   = velocitySrc->load (std::memory_order_relaxed);
        holdFrames     = kHoldFrames;
    }
    else if (holdFrames > 0)
    {
        --holdFrames;
    }
    else
    {
        displayLevel = juce::jmax (0.0f, displayLevel - kDecayPerFrame);
    }

    repaint();
}

void VelocityMeterComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    // Background.
    g.setColour (juce::Colour (0xff0d1520));
    g.fillRoundedRectangle (bounds, 2.0f);

    if (displayLevel > 0.005f)
    {
        const float fillH   = bounds.getHeight() * displayLevel;
        const auto  fillRect = bounds.withTop (bounds.getBottom() - fillH);

        // Light green (loud, top) → dark green (quiet, bottom).
        juce::ColourGradient grad (juce::Colour (0xff80ff60),   // light green at top
                                   fillRect.getCentreX(), fillRect.getY(),
                                   juce::Colour (0xff0a3010),   // dark green at bottom
                                   fillRect.getCentreX(), fillRect.getBottom(),
                                   false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fillRect, 2.0f);
    }

    // Subtle border.
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (bounds, 2.0f, 0.8f);
}
