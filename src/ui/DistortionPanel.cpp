#include "DistortionPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

DistortionPanel::DistortionPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::distEnabled, enableButton);

    modeBox.addItem ("Bit Crush",   1);
    modeBox.addItem ("Clip Hard",   2);
    modeBox.addItem ("Clip Soft",   3);
    modeBox.addItem ("Foldback",    4);
    modeBox.addItem ("Ring Mod",    5);
    modeBox.addItem ("Sat Hard",    6);
    modeBox.addItem ("Sat Soft",    7);
    modeBox.addItem ("Waveshaper",  8);
    addAndMakeVisible (modeBox);
    modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::distMode, modeBox);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id) {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob", true);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    driveAttach  = makeKnob (driveSlider,  driveLabel,  ParamIDs::distDrive);
    driveSlider .getProperties().set ("centreTextKnob", juce::var ("Amt"));
    driveLabel  .setVisible (false);

    outputAttach = makeKnob (outputSlider, outputLabel, ParamIDs::distOutput);
    outputSlider.getProperties().set ("centreTextKnob", juce::var ("Gain"));
    outputLabel .setVisible (false);

    bandLowAttach  = makeKnob (bandLowSlider,  bandLowLabel,  ParamIDs::distDriveLow);
    bandLowSlider .getProperties().set ("centreTextKnob", juce::var ("Lo"));
    bandLowLabel  .setVisible (false);

    bandMidAttach  = makeKnob (bandMidSlider,  bandMidLabel,  ParamIDs::distDriveMid);
    bandMidSlider .getProperties().set ("centreTextKnob", juce::var ("Mid"));
    bandMidLabel  .setVisible (false);

    bandHighAttach = makeKnob (bandHighSlider, bandHighLabel, ParamIDs::distDriveHigh);
    bandHighSlider.getProperties().set ("centreTextKnob", juce::var ("Hi"));
    bandHighLabel .setVisible (false);
    envAmtAttach   = makeKnob (envAmtSlider,   envAmtLabel,   ParamIDs::envDistAmt);
    envAmtSlider.getProperties().set ("fxKnob",   false);
    envAmtSlider.getProperties().set ("gateKnob", true);
    envAmtSlider  .getProperties().set ("centreTextKnob", juce::var ("Step"));
    envAmtLabel   .setVisible (false);
    stepModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (stepModIndicator);

    afEnvAmtAttach = makeKnob (afEnvAmtSlider, afEnvAmtLabel, ParamIDs::afEnvDistAmt);
    afEnvAmtSlider.getProperties().set ("envKnob", true);
    afEnvAmtSlider.getProperties().set ("centreTextKnob", juce::var ("Env"));
    afEnvAmtLabel .setVisible (false);

    for (int i = 2; i <= 16; ++i) bitDepthBox.addItem (juce::String (i), i - 1);
    addAndMakeVisible (bitDepthLabel);
    addAndMakeVisible (bitDepthBox);
    bitDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::distBitDepth, bitDepthBox);

    // Show bit-depth controls only in Bit Crush mode.
    auto applyBitCrushVisibility = [this]
    {
        const bool isBitCrush = (modeBox.getSelectedId() == 1);
        bitDepthLabel.setVisible (isBitCrush);
        bitDepthBox  .setVisible (isBitCrush);
    };
    modeBox.onChange = applyBitCrushVisibility;
    applyBitCrushVisibility();   // set correct state from restored parameter
}

void DistortionPanel::setModSources (std::atomic<float>* gateLevel,  std::atomic<float>* gateAmt,
                                      std::atomic<float>* afEnvLevel, std::atomic<float>* afEnvAmt,
                                      std::atomic<float>* driveBase)
{
    stepModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    stepModIndicator.setCenterSource (driveBase);
    stepModIndicator.setDestinationRange (0.0f, 1.0f);
    stepModIndicator.setSource (gateLevel, gateAmt);
    stepModIndicator.setSecondSource (afEnvLevel, afEnvAmt);
}

void DistortionPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void DistortionPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromLeft (2);  // 6 px total from left edge

    enableButton.setBounds (area.removeFromLeft (100).withSizeKeepingCentre (100, 24));
    area.removeFromLeft (8);
    modeBox.setBounds (area.removeFromLeft (115).withSizeKeepingCentre (115, 24));
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

    addKnob (driveLabel,    driveSlider);
    stepModIndicator.setBounds (driveSlider.getBounds());
    addKnob (bandLowLabel,  bandLowSlider);
    addKnob (bandMidLabel,  bandMidSlider);
    addKnob (bandHighLabel, bandHighSlider);
    addKnob (outputLabel,   outputSlider);
    addKnob (envAmtLabel,   envAmtSlider);
    addKnob (afEnvAmtLabel, afEnvAmtSlider);
    area.removeFromLeft (gap);

    // BitDepth: vertically centred combo box.
    const int areaTop = area.getY();
    const int comboH  = 24;
    const int comboY  = areaTop + (area.getHeight() - comboH) / 2;
    bitDepthLabel.setBounds (area.removeFromLeft (32).withY (comboY).withHeight (comboH));
    bitDepthBox.setBounds   (area.removeFromLeft (52).withY (comboY).withHeight (comboH));
}
