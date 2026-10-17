#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "LfoModIndicator.h"

// Stutter control strip: on/off toggle, probability knob,
// time-division ComboBox (updated in real time by LFO), three LFO knobs,
// Len knob with mode toggle, and a Len LFO section (waveform, Amt, Spd).
// Layout and visual style mirrors the Granulizer panel row.
class StutterPanel : public juce::Component,
                     private juce::Timer
{
public:
    explicit StutterPanel (juce::AudioProcessorValueTreeState& apvts);
    ~StutterPanel() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

    // Wire up the real-time division display.  Call once from PluginEditor.
    void setEffectiveDivSource (std::atomic<int>* source);

    // Wire up the Len LFO modulation indicator.  Call once from PluginEditor.
    void setLenLfoModSource (std::atomic<float>* lfoOutput,
                             std::atomic<float>* lfoDepth);

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& apvtsRef;
    std::atomic<int>* effectiveDivSource { nullptr };

    juce::ToggleButton enableButton { "Stutter" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::Label  amountLabel  { {}, "Amt" };
    juce::Slider amountSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttach;

    // Division LED display — read-only label with tooltip.
    juce::Label divDisplay;
    int cachedDivIdx_ { -1 };

    juce::Label  lfoLowLabel  { {}, "<" };
    juce::Slider lfoLowSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoLowAttach;

    juce::Label  lfoHighLabel { {}, ">" };
    juce::Slider lfoHighSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoHighAttach;

    juce::Label  lfoSpeedLabel { {}, "Spd" };
    juce::Slider lfoSpeedSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoSpeedAttach;

    juce::Label  midiLenLabel  { {}, "Len" };
    juce::Slider midiLenSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midiLenAttach;

    juce::ToggleButton lenModeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lenModeAttach;

    // Len LFO section — waveform combo + Amt knob + Spd knob + mod indicator.
    juce::ComboBox lenLfoWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lenLfoWaveAttach;

    juce::Label  lenLfoAmtLabel { {}, "LAmt" };
    juce::Slider lenLfoAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lenLfoAmtAttach;

    juce::Label  lenLfoSpdLabel { {}, "LSpd" };
    juce::Slider lenLfoSpdSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lenLfoSpdAttach;

    LfoModIndicator lenLfoModIndicator;
};
