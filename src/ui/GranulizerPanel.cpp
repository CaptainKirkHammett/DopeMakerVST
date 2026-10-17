#include "GranulizerPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

GranulizerPanel::GranulizerPanel (juce::AudioProcessorValueTreeState& apvts)
{
    // No fxButton property — same purple toggle style as the Step Controller button.
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::granEnabled, enableButton);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb,
                         const juce::String& paramId, const juce::String& centreText)
    {
        sl.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob",        true);
        sl.getProperties().set ("granKnob",       true);
        sl.getProperties().set ("centreTextKnob", juce::var (centreText));
        lb.setVisible (false);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramId, sl);
    };

    attackAttach      = makeKnob (attackSlider,      attackLabel,      ParamIDs::granAttack,      "Atk");
    holdAttach        = makeKnob (holdSlider,        holdLabel,        ParamIDs::granHold,        "Hold");
    spacingAttach     = makeKnob (spacingSlider,     spacingLabel,     ParamIDs::granSpacing,     "Spc");
    waveSpacingAttach = makeKnob (waveSpacingSlider, waveSpacingLabel, ParamIDs::granWaveSpacing, "WSpc");
    panAttach         = makeKnob (panSlider,         panLabel,         ParamIDs::granPan,         "Pan");
    depthAttach       = makeKnob (depthSlider,       depthLabel,       ParamIDs::granDepth,       "Amt");
    speedAttach       = makeKnob (speedSlider,       speedLabel,       ParamIDs::granSpeed,       "Spd");
    randAttach        = makeKnob (randSlider,        randLabel,        ParamIDs::granRand,        "Rnd");

    lfoModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (lfoModIndicator);
}

void GranulizerPanel::setModSources (std::atomic<float>* lfoOutput,
                                     std::atomic<float>* depthBase,
                                     std::atomic<float>* waveSpacingBase)
{
    lfoModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    lfoModIndicator.setModType (LfoModIndicator::ModType::MultiplicativeBipolar);
    lfoModIndicator.setBaseSource (waveSpacingBase);
    lfoModIndicator.setNormalisableRange (
        juce::NormalisableRange<float> (-3.0f, 3.0f));
    lfoModIndicator.setSource (lfoOutput, depthBase);
}

void GranulizerPanel::paint (juce::Graphics&)
{
    // No framing — granulizer controls sit directly on the main background.
}

void GranulizerPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 4);

    constexpr int btnW  = 80;
    constexpr int btnH  = 22;
    constexpr int knobH = 52;
    constexpr int numKnobs = 8;

    enableButton.setBounds (area.removeFromLeft (btnW).withSizeKeepingCentre (btnW, btnH));

    // Distribute knobs tightly across the remaining width.
    const int knobAreaW = area.getWidth();
    const int knobW = knobAreaW / numKnobs;

    juce::Slider* sliders[] = { &attackSlider, &holdSlider, &spacingSlider,
                                &waveSpacingSlider, &panSlider, &depthSlider,
                                &speedSlider, &randSlider };
    juce::Label*  labels[]  = { &attackLabel, &holdLabel, &spacingLabel,
                                &waveSpacingLabel, &panLabel, &depthLabel,
                                &speedLabel, &randLabel };

    for (int i = 0; i < numKnobs; ++i)
    {
        auto col = area.removeFromLeft (knobW);
        labels[i]->setBounds (col);
        sliders[i]->setBounds (col.withSizeKeepingCentre (juce::jmin (knobW, 48), knobH));
    }

    lfoModIndicator.setBounds (waveSpacingSlider.getBounds());
}
