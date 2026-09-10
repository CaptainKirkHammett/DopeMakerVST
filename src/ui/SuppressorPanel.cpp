#include "SuppressorPanel.h"
#include "../ParameterIDs.h"

SuppressorPanel::SuppressorPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::suppressorEnabled, enableButton);

    fftButton.getProperties().set ("fxButton", true);
    fftButton.setTooltip ("AutoEQ Mode\n\n"
                          "OFF  IIR (24-band): fast time-domain resonance suppression using "
                          "24 bandpass filters from 100 Hz to 14 kHz. Zero latency, low CPU.\n\n"
                          "ON   FFT (spectral): 512-point STFT with per-bin peak detection. "
                          "Much higher frequency resolution - catches narrow resonances that "
                          "IIR bands miss. Higher CPU cost and adds ~12 ms latency.");
    addAndMakeVisible (fftButton);
    fftAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::suppressorFFT, fftButton);

    amtSlider.setSliderStyle  (juce::Slider::LinearHorizontal);
    amtSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (amtSlider);
    amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::suppressorAmt, amtSlider);
}

void SuppressorPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void SuppressorPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    enableButton.setBounds (area.removeFromLeft (72).withSizeKeepingCentre (72, 24));
    area.removeFromLeft (4);
    fftButton.setBounds (area.removeFromLeft (42).withSizeKeepingCentre (42, 24));
    area.removeFromLeft (4);
    amtSlider.setBounds (area);
}
