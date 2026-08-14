#include "FilterPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

FilterPanel::FilterPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::filterEnabled, enableButton);

    modeBox.addItem ("Low-Pass",  1);
    modeBox.addItem ("High-Pass", 2);
    modeBox.addItem ("Band-Pass", 3);
    modeBox.addItem ("Comb+",     4);
    modeBox.addItem ("Comb-",     5);
    addAndMakeVisible (modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::filterMode, modeBox);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id) {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob", true);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    cutoffAttach    = makeKnob (cutoffSlider,    cutoffLabel,    ParamIDs::filterCutoff);
    cutoffSlider   .getProperties().set ("centreTextKnob", juce::var ("Cut"));
    cutoffLabel    .setVisible (false);

    resonanceAttach = makeKnob (resonanceSlider, resonanceLabel, ParamIDs::filterResonance);
    resonanceSlider.getProperties().set ("centreTextKnob", juce::var ("Res"));
    resonanceLabel .setVisible (false);

    lfoRateAttach   = makeKnob (lfoRateSlider,   lfoRateLabel,   ParamIDs::filterLfoRate);
    lfoRateSlider  .getProperties().set ("centreTextKnob", juce::var ("LFO\nRate"));
    lfoRateLabel   .setVisible (false);

    lfoDepthAttach  = makeKnob (lfoDepthSlider,  lfoDepthLabel,  ParamIDs::filterLfoDepth);
    lfoDepthSlider .getProperties().set ("centreTextKnob", juce::var ("LFO\nAmt"));
    lfoDepthLabel  .setVisible (false);

    lfoWaveAttach = makeKnob (lfoWaveSlider, lfoWaveLabel, ParamIDs::filterLfoWaveform);
    lfoWaveSlider.getProperties().set ("centreTextKnob", juce::var ("wave"));
    lfoWaveLabel .setVisible (false);

    envAmtAttach    = makeKnob (envAmtSlider,    envAmtLabel,    ParamIDs::envFilterAmt);
    envAmtSlider.getProperties().set ("fxKnob",   false);
    envAmtSlider.getProperties().set ("gateKnob", true);
    envAmtSlider   .getProperties().set ("centreTextKnob", juce::var ("Step"));
    envAmtLabel    .setVisible (false);
    stepModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (stepModIndicator);

    lfoModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (lfoModIndicator);

    afEnvAmtAttach = makeKnob (afEnvAmtSlider, afEnvAmtLabel, ParamIDs::afEnvFilterAmt);
    afEnvAmtSlider.getProperties().set ("envKnob", true);
    afEnvAmtSlider .getProperties().set ("centreTextKnob", juce::var ("Env"));
    afEnvAmtLabel  .setVisible (false);
}

void FilterPanel::setModSources (std::atomic<float>* gateLevel,  std::atomic<float>* gateAmt,
                                  std::atomic<float>* afEnvLevel, std::atomic<float>* afEnvAmt,
                                  std::atomic<float>* cutoffBase,
                                  std::atomic<float>* lfoOutput,  std::atomic<float>* lfoDepth)
{
    // Cutoff modulation is in octaves: raw = baseCutoff × 2^(signal × amount × 4).
    stepModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    stepModIndicator.setModType (LfoModIndicator::ModType::ExponentialOctave);
    stepModIndicator.setCenterSource (cutoffBase);
    stepModIndicator.setNormalisableRange (juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f));
    stepModIndicator.setSource (gateLevel, gateAmt);
    stepModIndicator.setSecondSource (afEnvLevel, afEnvAmt);

    // Filter LFO → Cutoff knob (exponential octave, same formula as the actual DSP).
    lfoModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    lfoModIndicator.setModType (LfoModIndicator::ModType::ExponentialOctave);
    lfoModIndicator.setCenterSource (cutoffBase);
    lfoModIndicator.setNormalisableRange (juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f));
    lfoModIndicator.setSource (lfoOutput, lfoDepth);
}

void FilterPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void FilterPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromLeft (2);  // 6 px total from left edge

    enableButton.setBounds (area.removeFromLeft (100).withSizeKeepingCentre (100, 24));
    area.removeFromLeft (8);
    modeBox.setBounds (area.removeFromLeft (115).withSizeKeepingCentre (115, 24));
    area.removeFromLeft (8);

    constexpr int knobW = 61;
    constexpr int knobH = 52;   // match the effective knob size of Dist/Flanger (60 px row − 8 reduced)
    constexpr int gap   = 2;
    constexpr int lblH  = 14;

    auto addKnob = [&] (juce::Label& lb, juce::Slider& sl) {
        auto col = area.removeFromLeft (knobW);
        if (!sl.getProperties().contains ("centreTextKnob"))
        {
            lb.setBounds (col.removeFromTop (lblH));
            lb.setJustificationType (juce::Justification::centred);
        }
        sl.setBounds (col.withSizeKeepingCentre (knobW, knobH));
        area.removeFromLeft (gap);
    };

    addKnob (cutoffLabel,    cutoffSlider);
    stepModIndicator.setBounds (cutoffSlider.getBounds());
    lfoModIndicator .setBounds (cutoffSlider.getBounds());
    addKnob (resonanceLabel, resonanceSlider);
    addKnob (lfoRateLabel,   lfoRateSlider);
    addKnob (lfoDepthLabel,  lfoDepthSlider);
    addKnob (lfoWaveLabel,   lfoWaveSlider);
    addKnob (envAmtLabel,    envAmtSlider);
    addKnob (afEnvAmtLabel,  afEnvAmtSlider);
}
