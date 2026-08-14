#include "GranulizerProcessor.h"
#include <limits>

void GranulizerProcessor::prepare (double sr, int maxBlockSize)
{
    sampleRate      = sr;
    // Power-of-two buffer so we can wrap with a bitwise AND instead of modulo.
    delayBufferSize = juce::nextPowerOfTwo (static_cast<int> (kBufferSecs * sr));
    delayBuffer .setSize (2, delayBufferSize, false, true, false);
    scratchBuffer.setSize (2, maxBlockSize,   false, true, false);
    writeHead      = 0;
    schedulerTimer = 0.0f;
    lfoPhase       = 0.0f;
    grainCounter   = 0;
    for (auto& g : grains)
        g.active = false;
}

void GranulizerProcessor::process (juce::AudioBuffer<float>& buffer,
                                    bool  enabled,
                                    float attackMs,
                                    float holdMs,
                                    float spacingMs,
                                    float waveSpacing,
                                    float panAmt,
                                    float depth,
                                    float speed,
                                    float randAmt)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);

    // Preserve input so the delay buffer keeps filling even when disabled.
    for (int ch = 0; ch < numChannels; ++ch)
        scratchBuffer.copyFrom (ch, 0, buffer.getReadPointer (ch), numSamples);

    const float attackSmpF  = attackMs  * static_cast<float> (sampleRate) * 0.001f;
    const float holdSmpF    = holdMs    * static_cast<float> (sampleRate) * 0.001f;
    const float spacingSmpF = spacingMs * static_cast<float> (sampleRate) * 0.001f;

    // LFO phase increment per sample: speed is in Hz.
    const float lfoPhaseInc = juce::MathConstants<float>::twoPi * speed
                              / static_cast<float> (sampleRate);

    if (enabled)
        buffer.clear();

    const float dbSzF = static_cast<float> (delayBufferSize);

    for (int s = 0; s < numSamples; ++s)
    {
        // Write this input sample into the circular delay buffer.
        for (int ch = 0; ch < numChannels; ++ch)
            delayBuffer.setSample (ch, writeHead, scratchBuffer.getSample (ch, s));
        writeHead = (writeHead + 1) & (delayBufferSize - 1);

        if (!enabled)
            continue;

        // Advance internal wave-spacing LFO.
        lfoPhase += lfoPhaseInc;
        if (lfoPhase >= juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;

        // Grain scheduler — fire on the first sample and on every elapsed interval.
        schedulerTimer -= 1.0f;
        if (schedulerTimer <= 0.0f)
        {
            // Effective wave spacing = base + LFO modulation (depth scales ±waveSpacing).
            const float lfoVal = std::sin (lfoPhase);
            const float effectiveWS = waveSpacing + lfoVal * depth * waveSpacing;

            const int atkSmp  = juce::jmax (1, static_cast<int> (attackSmpF));
            const int hldSmp  = juce::jmax (1, static_cast<int> (holdSmpF));
            spawnGrain (effectiveWS, panAmt, randAmt, atkSmp, hldSmp);

            // Randomise spacing ±50 % when randAmt > 0.
            const float jitter = randAmt * (rng.nextFloat() * 2.0f - 1.0f) * spacingSmpF * 0.5f;
            schedulerTimer = juce::jmax (1.0f, spacingSmpF + jitter);
        }

        // Mix all active grains into the output buffer.
        for (auto& g : grains)
        {
            if (!g.active)
                continue;

            const float env = grainEnvelope (g);

            const float sL = readInterp (0, g.readPos);
            const float sR = (numChannels > 1) ? readInterp (1, g.readPos) : sL;

            if (numChannels > 0)
                buffer.addSample (0, s, env * sL * g.panL);
            if (numChannels > 1)
                buffer.addSample (1, s, env * sR * g.panR);

            g.readPos += g.playRate;
            // Wrap in both directions (playRate can be negative).
            if (g.readPos >= dbSzF)
                g.readPos -= dbSzF;
            else if (g.readPos < 0.0f)
                g.readPos += dbSzF;

            ++g.age;
            if (g.age >= g.totalSmp)
                g.active = false;
        }
    }

    // Expose the time-offset of the most recently spawned active grain for UI display.
    if (enabled)
    {
        float newestPos = -1.0f;
        int   youngestAge = std::numeric_limits<int>::max();
        for (const auto& g : grains)
        {
            if (g.active && g.age < youngestAge)
            {
                youngestAge = g.age;
                newestPos = g.readPos;
            }
        }
        if (newestPos >= 0.0f)
        {
            // How many samples behind the write head is this grain reading?
            float behind = static_cast<float> (writeHead) - newestPos;
            if (behind < 0.0f)
                behind += static_cast<float> (delayBufferSize);
            grainOffsetSeconds.store (behind / static_cast<float> (sampleRate), std::memory_order_relaxed);
        }
        else
        {
            grainOffsetSeconds.store (-1.0f, std::memory_order_relaxed);
        }
    }
    else
    {
        grainOffsetSeconds.store (-1.0f, std::memory_order_relaxed);
    }
}

void GranulizerProcessor::spawnGrain (float effectiveWaveSpacing, float panAmt,
                                       float randAmt, int attackSmp, int holdSmp)
{
    // Find the first inactive slot.
    Grain* slot = nullptr;
    for (auto& g : grains)
    {
        if (!g.active)
        {
            slot = &g;
            break;
        }
    }
    if (slot == nullptr)
        return; // all slots occupied — drop this grain

    // Read position: offset just enough behind the write head to cover the grain
    // attack ramp, so the grain reads freshly written audio and stays in time.
    const float baseOffset = static_cast<float> (attackSmp + 64);
    float startPos = static_cast<float> (writeHead) - baseOffset;
    if (randAmt > 0.0f)
        startPos += (rng.nextFloat() * 2.0f - 1.0f) * randAmt * 512.0f;

    // Wrap into [0, delayBufferSize).
    const float dbSzF = static_cast<float> (delayBufferSize);
    while (startPos <    0.0f)  startPos += dbSzF;
    while (startPos >= dbSzF)   startPos -= dbSzF;

    slot->readPos    = startPos;
    slot->attackSmp  = attackSmp;
    slot->holdSmp    = holdSmp;
    slot->releaseSmp = attackSmp;   // symmetric: release mirrors attack duration
    slot->totalSmp   = attackSmp + holdSmp + attackSmp;
    slot->age        = 0;
    slot->playRate   = effectiveWaveSpacing;  // stride through the waveform; negative = reverse
    slot->active     = true;

    // Alternating L/R pan: odd grains go left, even grains go right.
    const float panDir = ((grainCounter & 1) == 0) ? -1.0f : 1.0f;
    const float panRaw = panDir * panAmt;
    const float angle  = (panRaw + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi;
    slot->panL = std::cos (angle);
    slot->panR = std::sin (angle);

    ++grainCounter;
}

float GranulizerProcessor::grainEnvelope (const Grain& g) const
{
    if (g.age < g.attackSmp)
    {
        // sin² ramp: starts at 0 with slope 0, reaches 1 with slope 0 — no transient at onset.
        const float t = static_cast<float> (g.age) / static_cast<float> (g.attackSmp);
        const float s = std::sin (t * juce::MathConstants<float>::halfPi);
        return s * s;
    }

    const int holdEnd = g.attackSmp + g.holdSmp;
    if (g.age < holdEnd)
        return 1.0f;

    if (g.age < g.totalSmp)
    {
        // cos² ramp: symmetric decay — starts at 1 with slope 0, ends at 0 with slope 0.
        const float t = static_cast<float> (g.age - holdEnd)
                        / static_cast<float> (g.releaseSmp);
        const float c = std::cos (t * juce::MathConstants<float>::halfPi);
        return c * c;
    }

    return 0.0f;
}

float GranulizerProcessor::readInterp (int channel, float pos) const
{
    const int   i0   = static_cast<int> (pos) & (delayBufferSize - 1);
    const int   i1   = (i0 + 1)               & (delayBufferSize - 1);
    const float frac = pos - std::floor (pos);
    const float* d   = delayBuffer.getReadPointer (channel);
    return d[i0] * (1.0f - frac) + d[i1] * frac;
}
