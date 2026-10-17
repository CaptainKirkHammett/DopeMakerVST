#include "GoniometerComponent.h"

GoniometerComponent::GoniometerComponent()
{
    setInterceptsMouseClicks (false, false);
}

void GoniometerComponent::setSource (StereoFieldSource* src)
{
    source = src;
    if (src != nullptr)
        startTimerHz (30);
    else
        stopTimer();
}

// ── Timer ─────────────────────────────────────────────────────────────────────

void GoniometerComponent::timerCallback()
{
    if (source == nullptr)
        return;

    const int got = source->pullSamples (pullL, pullR, kPullSize);
    if (got <= 0)
        return;

    for (int i = 0; i < got; i += kDecimate)
    {
        const float l = pullL[i], r = pullR[i];
        // Skip corrupt/transient samples produced during file transitions.
        if (!std::isfinite (l) || !std::isfinite (r))
            continue;

        ring[writeIdx] = { l, r };
        writeIdx       = (writeIdx + 1) % kMaxPoints;
        if (totalPts < kMaxPoints)
            ++totalPts;
    }

    repaint();
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void GoniometerComponent::paint (juce::Graphics& g)
{
    const float cx     = getWidth()  * 0.5f;
    const float cy     = getHeight() * 0.5f;
    const float innerR = juce::jmin (cx, cy) * 0.66f;

    // Clip all drawing to the inner knob circle.
    juce::Path clip;
    clip.addEllipse (cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (clip);

    // Faint crosshairs — Mid (vertical) and Side (horizontal) axes.
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.drawLine (cx - innerR, cy,          cx + innerR, cy,          0.5f);
    g.drawLine (cx,          cy - innerR, cx,          cy + innerR, 0.5f);

    // Diagonal L / R channel guides (45°).
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    const float d = innerR * 0.7071f;
    g.drawLine (cx - d, cy + d, cx + d, cy - d, 0.5f);
    g.drawLine (cx - d, cy - d, cx + d, cy + d, 0.5f);

    if (totalPts == 0)
        return;

    // Scale: full-scale mono (L=R=1 → M+S=√2) reaches innerR.
    const float scale = innerR / 1.4142f;

    // Phosphor cyan dot colour.
    const juce::Colour dotColour (0xff39ff14); // neon green — matches knob pointer colour

    // Draw all stored points, newest first with brightest alpha.
    // writeIdx-1 is the newest entry.
    for (int age = 0; age < totalPts; ++age)
    {
        const int idx = (writeIdx - 1 - age + kMaxPoints) % kMaxPoints;

        // Quadratic falloff: alpha = 1 when age=0, 0 when age=totalPts-1.
        const float t     = 1.0f - static_cast<float> (age) / static_cast<float> (kMaxPoints);
        const float alpha = t * t * 0.9f;  // peak 0.9 for newest dot
        if (alpha < 0.01f)
            break;

        const float L  = juce::jlimit (-2.0f, 2.0f, ring[idx].l);
        const float R  = juce::jlimit (-2.0f, 2.0f, ring[idx].r);
        const float px = cx + (L - R) * scale;
        const float py = cy - (L + R) * scale;

        g.setColour (dotColour.withAlpha (alpha));
        g.fillRect (px - 0.5f, py - 0.5f, 2.0f, 2.0f);
    }
}
