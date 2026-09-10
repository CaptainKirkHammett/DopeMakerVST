#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "LfoModIndicator.h"

// FL-Studio-inspired granulizer control bar.
// Spans roughly half the plugin width; contains an on/off toggle followed by eight
// rotary knobs: Attack, Hold, Spacing, WSpc, Pan, Depth, Speed, Rand.
// All knobs use the fxKnob / centreTextKnob style (label drawn inside the knob).
class GranulizerPanel : public juce::Component
{
public:
    explicit GranulizerPanel (juce::AudioProcessorValueTreeState& apvts);

    void setModSources (std::atomic<float>* lfoOutput,
                        std::atomic<float>* depthBase,
                        std::atomic<float>* waveSpacingBase);

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    juce::ToggleButton enableButton { "Granulizer" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttach;

    juce::Label  attackLabel  { {}, "Attack"  };
    juce::Slider attackSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;

    juce::Label  holdLabel    { {}, "Hold"    };
    juce::Slider holdSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttach;

    juce::Label  spacingLabel { {}, "Spacing" };
    juce::Slider spacingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spacingAttach;

    juce::Label  waveSpacingLabel { {}, "WSpc" };
    juce::Slider waveSpacingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> waveSpacingAttach;

    juce::Label  panLabel     { {}, "Pan"     };
    juce::Slider panSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttach;

    juce::Label  depthLabel   { {}, "Depth"   };
    juce::Slider depthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach;

    juce::Label  speedLabel   { {}, "Speed"   };
    juce::Slider speedSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttach;

    juce::Label  randLabel    { {}, "Rand"    };
    juce::Slider randSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> randAttach;

    LfoModIndicator lfoModIndicator;
};
