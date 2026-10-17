#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// A button-sized fader for the Duck amount parameter (0.0 – 1.0).
// Visually the fill rises from the bottom: fully dark at 0.0 (off),
// fully bright at 1.0 (maximum), using the same disabled/enabled purple
// tints as the standard toggle buttons.  Vertical drag sets the value.
class DuckFaderButton : public juce::Component,
                        public juce::SettableTooltipClient,
                        private juce::Slider::Listener
{
public:
    DuckFaderButton (juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& paramId,
                     const juce::String& label = "Duck");

    void paint   (juce::Graphics&)                                          override;
    void mouseDown  (const juce::MouseEvent&)                               override;
    void mouseDrag  (const juce::MouseEvent&)                               override;
    void mouseUp    (const juce::MouseEvent&)                               override;
    void mouseWheelMove (const juce::MouseEvent&,
                         const juce::MouseWheelDetails&)                    override;
    void mouseDoubleClick (const juce::MouseEvent&)                         override;

private:
    void sliderValueChanged (juce::Slider* s) override;

    // Hidden slider owns the APVTS attachment; we read/write through it.
    juce::Slider hiddenSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;

    float dragStartValue { 0.0f };
    int   dragStartY     { 0 };
    bool  isPressed      { false };

    juce::String labelText;

    // Pixels of vertical drag that spans the full 0→1 range.
    static constexpr float kDragRange = 80.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DuckFaderButton)
};
