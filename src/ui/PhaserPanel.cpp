#include "PhaserPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

PhaserPanel::PhaserPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::phaserEnabled, enableButton);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id) {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob", true);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    rateAttach   = makeKnob (rateSlider,   rateLabel,   ParamIDs::phaserRate);
    rateSlider  .getProperties().set ("centreTextKnob", juce::var ("LFO\nRate"));
    rateLabel   .setVisible (false);

    depthAttach  = makeKnob (depthSlider,  depthLabel,  ParamIDs::phaserDepth);
    depthSlider .getProperties().set ("centreTextKnob", juce::var ("Amt"));
    depthLabel  .setVisible (false);

    fbAttach     = makeKnob (fbSlider,     fbLabel,     ParamIDs::phaserFeedback);
    fbSlider    .getProperties().set ("centreTextKnob", juce::var ("FB"));
    fbLabel     .setVisible (false);

    centreAttach = makeKnob (centreSlider, centreLabel, ParamIDs::phaserCentreFreq);
    centreSlider.getProperties().set ("centreTextKnob", juce::var ("Cntr"));
    centreLabel .setVisible (false);

    mixAttach    = makeKnob (mixSlider,    mixLabel,    ParamIDs::phaserMix);
    mixSlider   .getProperties().set ("centreTextKnob", juce::var ("Mix"));
    mixLabel    .setVisible (false);
    envAmtAttach   = makeKnob (envAmtSlider,   envAmtLabel,   ParamIDs::envPhaserAmt);
    envAmtSlider.getProperties().set ("fxKnob",   false);
    envAmtSlider.getProperties().set ("gateKnob", true);
    envAmtSlider  .getProperties().set ("centreTextKnob", juce::var ("Step"));
    envAmtLabel   .setVisible (false);
    stepModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (stepModIndicator);

    afEnvAmtAttach = makeKnob (afEnvAmtSlider, afEnvAmtLabel, ParamIDs::afEnvPhaserAmt);
    afEnvAmtSlider.getProperties().set ("envKnob", true);
    afEnvAmtSlider.getProperties().set ("centreTextKnob", juce::var ("Env"));
    afEnvAmtLabel .setVisible (false);
    envModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (envModIndicator);

    lfoModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (lfoModIndicator);

    for (int s : { 2, 4, 6, 8, 10, 12 })
        stagesBox.addItem (juce::String (s), s / 2);
    stagesLabel.setVisible (false);
    addAndMakeVisible (stagesLabel);
    addAndMakeVisible (stagesBox);
    stagesAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::phaserStages, stagesBox);
}

void PhaserPanel::setModSources (std::atomic<float>* gateLevel,    std::atomic<float>* gateAmt,
                                  std::atomic<float>* afEnvLevel,  std::atomic<float>* afEnvAmt,
                                  std::atomic<float>* mixBase,     std::atomic<float>* fbBase,
                                  std::atomic<float>* lfoOutput,   std::atomic<float>* depthBase,
                                  std::atomic<float>* centreFreqBase)
{
    // Step modulates Mix.
    stepModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    stepModIndicator.setCenterSource (mixBase);
    stepModIndicator.setDestinationRange (0.0f, 1.0f);
    stepModIndicator.setSource (gateLevel, gateAmt);

    // Env modulates Feedback.
    envModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    envModIndicator.setCenterSource (fbBase);
    envModIndicator.setDestinationRange (-0.99f, 0.99f);
    envModIndicator.setSource (afEnvLevel, afEnvAmt);

    // Phaser LFO → Centre Freq knob: shows centreFreq × (1 + depth × lfoOut),
    // i.e. where the allpass sweep centre effectively is during the LFO cycle.
    lfoModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    lfoModIndicator.setModType (LfoModIndicator::ModType::MultiplicativeBipolar);
    lfoModIndicator.setBaseSource (centreFreqBase);
    lfoModIndicator.setNormalisableRange (
        juce::NormalisableRange<float> (200.0f, 20000.0f, 0.0f, 0.3f));
    lfoModIndicator.setSource (lfoOutput, depthBase);
}

void PhaserPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void PhaserPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromLeft (2);

    enableButton.setBounds (area.removeFromLeft (100).withSizeKeepingCentre (100, 24));
    area.removeFromLeft (8);
    stagesBox.setBounds (area.removeFromLeft (115).withSizeKeepingCentre (115, 24));
    area.removeFromLeft (8);

    constexpr int knobW = 61;
    constexpr int gap   = 2;
    constexpr int lblH  = 14;

    auto addKnob = [&] (juce::Label& lb, juce::Slider& sl) {
        auto col = area.removeFromLeft (knobW);
        if (!sl.getProperties().contains ("centreTextKnob"))
        {
            lb.setBounds (col.removeFromTop (lblH));
            lb.setJustificationType (juce::Justification::centred);
        }
        sl.setBounds (col);
        area.removeFromLeft (gap);
    };

    addKnob (depthLabel,  depthSlider);
    addKnob (fbLabel,     fbSlider);
    envModIndicator.setBounds (fbSlider.getBounds());
    addKnob (centreLabel, centreSlider);
    addKnob (rateLabel,   rateSlider);
    lfoModIndicator.setBounds (centreSlider.getBounds());
    addKnob (mixLabel,    mixSlider);
    stepModIndicator.setBounds (mixSlider.getBounds());
    addKnob (envAmtLabel,   envAmtSlider);
    addKnob (afEnvAmtLabel, afEnvAmtSlider);
}
