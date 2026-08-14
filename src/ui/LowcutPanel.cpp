#include "LowcutPanel.h"
#include "../ParameterIDs.h"

LowcutPanel::LowcutPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::lowcutEnabled, enableButton);

    freqSlider.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
    freqSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    freqSlider.getProperties().set ("fxKnob", true);
    freqSlider.getProperties().set ("centreTextKnob", juce::var ("Freq"));
    freqLabel.setVisible (false);
    addAndMakeVisible (freqLabel);
    addAndMakeVisible (freqSlider);
    freqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::lowcutFreq, freqSlider);

    amtSlider.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
    amtSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    amtSlider.getProperties().set ("fxKnob", true);
    amtSlider.getProperties().set ("centreTextKnob", juce::var ("stageDb"));
    addAndMakeVisible (amtLabel);
    addAndMakeVisible (amtSlider);
    amtLabel.setVisible (false);
    amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::lowcutAmt, amtSlider);

    amtSlider.textFromValueFunction = [] (double v) {
        const int stages = juce::jlimit (1, 8, static_cast<int> (v * 8) + 1);
        return juce::String (stages * 12) + " dB";
    };

    freqSlider.onValueChange  = [this] { repaint(); };
    amtSlider.onValueChange   = [this] { repaint(); };
    enableButton.onClick      = [this] { repaint(); };
}

void LowcutPanel::setSpectrumSource (SpectrumSource* source)
{
    spectrumSource = source;
    if (source != nullptr)
        startTimerHz (30);
    else
        stopTimer();
}

void LowcutPanel::timerCallback()
{
    if (spectrumSource != nullptr && spectrumSource->pullSpectrum (spectrumMags))
    {
        hasSpectrumData = true;
        repaint (filterVisualBounds);
    }
}

void LowcutPanel::drawSpectrum (juce::Graphics& g, juce::Rectangle<float> vb) const
{
    if (spectrumSource == nullptr)
        return;

    // Match the lowcut filter visual frequency range.
    const float minFreq  = 20.0f;
    const float maxFreq  = 2000.0f;
    const float logRange = std::log (maxFreq / minFreq);

    const float sr    = (float) spectrumSource->getSampleRate();
    const float binHz = sr / (float) SpectrumSource::kFftSize;

    juce::Path fill, line;
    bool started = false;

    for (int bin = 1; bin < SpectrumSource::kNumBins; ++bin)
    {
        const float freq = (float) bin * binHz;
        if (freq < minFreq || freq > maxFreq)
            continue;

        const float xNorm = std::log (freq / minFreq) / logRange;
        const float x     = vb.getX() + xNorm * vb.getWidth();
        const float y     = vb.getBottom() - spectrumMags[bin] * vb.getHeight();

        if (! started)
        {
            fill.startNewSubPath (x, vb.getBottom());
            fill.lineTo (x, y);
            line.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            fill.lineTo (x, y);
            line.lineTo (x, y);
        }
    }

    if (! started)
        return;

    fill.lineTo (vb.getRight(), vb.getBottom());
    fill.closeSubPath();

    g.setColour (juce::Colour (0xffff8800).withAlpha (0.25f));
    g.fillPath (fill);

    g.setColour (juce::Colour (0xffff8800).withAlpha (0.65f));
    g.strokePath (line, juce::PathStrokeType (1.0f));
}

void LowcutPanel::paint (juce::Graphics& g)
{
    // Same background as the rest of the FX section.
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    if (filterVisualBounds.isEmpty())
        return;

    auto vb = filterVisualBounds.toFloat();
    const int w = (int) vb.getWidth();

    if (w < 4)
        return;

    // Dark LCD-style background for the mini display.
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (vb, 2.0f);

    // Spectrum analyzer — drawn behind the filter curve.
    if (hasSpectrumData)
        drawSpectrum (g, vb);

    // Dim the curve when the effect is disabled.
    juce::Graphics::ScopedSaveState scopedState (g);
    if (! enableButton.getToggleState())
        g.setOpacity (0.35f);

    // Filter parameters — matches ToneFilterProcessor exactly.
    // Q is fixed at Butterworth (no boost); Amt selects the number of cascaded stages.
    const float fcHz = juce::jmax (1.0f, (float) freqSlider.getValue());
    const float amt  = (float) amtSlider.getValue();
    // Mirror the DSP stage count (1–8 stages, threshold every 1/8 of Amt range).
    constexpr int kMaxStages = 8;
    const int numStages = juce::jlimit (1, kMaxStages,
                                        static_cast<int> (amt * kMaxStages) + 1);
    constexpr float Q = 0.7071067811865476f;   // 1/√2, Butterworth

    // Display covers 20 Hz – 2 000 Hz on a logarithmic X axis.
    const float minFreq = 20.0f;
    const float maxFreq = 2000.0f;

    // Y axis: −96 dB (bottom) to +3 dB (top) — covers all 8 stages at full rolloff.
    const float minDb = -96.0f;
    const float maxDb =   3.0f;

    // Compute the magnitude response at each pixel column.
    float ys[512];
    const int  wClamped = juce::jmin (w, 512);
    for (int px = 0; px < wClamped; ++px)
    {
        const float fHz   = minFreq * std::pow (maxFreq / minFreq, (float) px / (wClamped - 1));
        const float omega = fHz / fcHz;
        const float om2   = omega * omega;
        // Single 2nd-order Butterworth highpass: |H| = ω² / sqrt((1-ω²)² + (ω/Q)²)
        const float denom      = std::sqrt ((1.0f - om2) * (1.0f - om2) + om2 / (Q * Q));
        const float singleGain = om2 / juce::jmax (1e-6f, denom);
        // Cascade N stages: overall gain = singleGain^N
        const float gain   = std::pow (singleGain, (float) numStages);
        const float gainDb = 20.0f * std::log10 (juce::jmax (1e-6f, gain));
        const float yNorm  = 1.0f - juce::jlimit (0.0f, 1.0f, (gainDb - minDb) / (maxDb - minDb));
        ys[px] = vb.getY() + yNorm * vb.getHeight();
    }

    // Filled area under the curve (passband / rolloff region).
    {
        juce::Path fill;
        fill.startNewSubPath (vb.getX(),       vb.getBottom());
        fill.lineTo           (vb.getX(),       ys[0]);
        for (int px = 1; px < wClamped; ++px)
            fill.lineTo (vb.getX() + px, ys[px]);
        fill.lineTo (vb.getRight(), vb.getBottom());
        fill.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillPath  (fill);
    }

    // Curve outline — neon green distinguishes lowcut from highcut.
    {
        juce::Path line;
        line.startNewSubPath (vb.getX(), ys[0]);
        for (int px = 1; px < wClamped; ++px)
            line.lineTo (vb.getX() + px, ys[px]);
        g.setColour (juce::Colour (0xffff1744).withAlpha (0.90f));
        g.strokePath (line, juce::PathStrokeType (1.5f));
    }

    // 0 dB reference line (passband ceiling).
    const float zeroY = vb.getY() + vb.getHeight() * (maxDb / (maxDb - minDb));
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine ((int) zeroY, vb.getX(), vb.getRight());

    // Outer border.
    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.drawRoundedRectangle (vb, 2.0f, 0.5f);
}

void LowcutPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    enableButton.setBounds (area.removeFromLeft (72).withSizeKeepingCentre (72, 24));
    area.removeFromLeft (6);
    freqLabel.setBounds ({});
    constexpr int knobSz = 47;
    constexpr int knobW  = 50;   // tight slot — no gap between freq and amt
    freqSlider.setBounds (area.removeFromLeft (knobW).withSizeKeepingCentre (knobSz, knobSz));
    amtSlider.setBounds  (area.removeFromLeft (knobW).withSizeKeepingCentre (knobSz, knobSz));

    // Remaining space holds the mini frequency-response visual, 10 % shorter than the row.
    auto fb = area.reduced (2);
    filterVisualBounds = fb.withSizeKeepingCentre (fb.getWidth(), juce::roundToInt (fb.getHeight() * 0.9f));
}
