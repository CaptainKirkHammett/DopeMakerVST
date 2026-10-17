#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../SpectrumSource.h"

class LowcutPanel : public juce::Component,
                    private juce::Timer
{
public:
    explicit LowcutPanel (juce::AudioProcessorValueTreeState& apvts);
    ~LowcutPanel() override;
    void paint   (juce::Graphics&) override;
    void resized () override;

    void setSpectrumSource (SpectrumSource* source);

private:
    juce::AudioProcessorValueTreeState& apvts;

    juce::ToggleButton enableButton { "Lowcut" };
    juce::RangedAudioParameter* modeParam { nullptr };

    juce::Label  freqLabel  { {}, "Freq" };
    juce::Slider freqSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach;

    juce::Label  amtLabel  { {}, "Amt" };
    juce::Slider amtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach;

    juce::Rectangle<int> filterVisualBounds;

    SpectrumSource* spectrumSource { nullptr };
    std::array<float, SpectrumSource::kNumBins> spectrumMags {};
    bool hasSpectrumData { false };

    void timerCallback() override;
    void drawSpectrum (juce::Graphics&, juce::Rectangle<float> vb) const;
    void showModeMenu();
    void syncButtonState();
};
