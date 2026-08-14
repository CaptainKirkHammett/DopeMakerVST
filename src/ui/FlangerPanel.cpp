#include "FlangerPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"
#include <BinaryData.h>

FlangerPanel::FlangerPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::flangerEnabled, enableButton);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id) {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob", true);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    rateAttach       = makeKnob (rateSlider,       rateLabel,       ParamIDs::flangerRate);
    rateSlider      .getProperties().set ("centreTextKnob", juce::var ("LFO\nRate"));
    rateLabel       .setVisible (false);

    feedbackAttach   = makeKnob (feedbackSlider,   feedbackLabel,   ParamIDs::flangerFeedback);
    feedbackSlider  .getProperties().set ("centreTextKnob", juce::var ("FB"));
    feedbackLabel   .setVisible (false);

    delayStartAttach = makeKnob (delayStartSlider, delayStartLabel, ParamIDs::flangerDelayStart);
    delayStartSlider.getProperties().set ("centreTextKnob", juce::var ("LFO\nSt"));
    delayStartLabel .setVisible (false);

    delayEndAttach   = makeKnob (delayEndSlider,   delayEndLabel,   ParamIDs::flangerDelayEnd);
    delayEndSlider  .getProperties().set ("centreTextKnob", juce::var ("LFO\nEd"));
    delayEndLabel   .setVisible (false);

    mixAttach        = makeKnob (mixSlider,        mixLabel,        ParamIDs::flangerMix);
    mixSlider       .getProperties().set ("centreTextKnob", juce::var ("Mix"));
    mixLabel        .setVisible (false);
    envAmtAttach     = makeKnob (envAmtSlider,     envAmtLabel,     ParamIDs::envFlangerAmt);
    envAmtSlider.getProperties().set ("fxKnob",   false);
    envAmtSlider.getProperties().set ("gateKnob", true);
    envAmtSlider  .getProperties().set ("centreTextKnob", juce::var ("Step"));
    envAmtLabel   .setVisible (false);
    stepModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (stepModIndicator);

    afEnvAmtAttach   = makeKnob (afEnvAmtSlider, afEnvAmtLabel, ParamIDs::afEnvFlangerAmt);
    afEnvAmtSlider.getProperties().set ("envKnob", true);
    afEnvAmtSlider.getProperties().set ("centreTextKnob", juce::var ("Env"));
    afEnvAmtLabel .setVisible (false);
    envModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (envModIndicator);

    lfoModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (lfoModIndicator);

    omImage  = juce::ImageCache::getFromMemory (BinaryData::om_png,     BinaryData::om_pngSize);
    eyeImage = juce::ImageCache::getFromMemory (BinaryData::alleye_png, BinaryData::alleye_pngSize);
}

void FlangerPanel::setModSources (std::atomic<float>* gateLevel,    std::atomic<float>* gateAmt,
                                   std::atomic<float>* afEnvLevel,  std::atomic<float>* afEnvAmt,
                                   std::atomic<float>* mixBase,     std::atomic<float>* feedbackBase,
                                   std::atomic<float>* lfoDelayDelta, std::atomic<float>* lfoFullDepth,
                                   std::atomic<float>* delayStartBase)
{
    // Step modulates Mix.
    stepModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    stepModIndicator.setCenterSource (mixBase);
    stepModIndicator.setDestinationRange (0.0f, 1.0f);
    stepModIndicator.setSource (gateLevel, gateAmt);

    // Env modulates Feedback.
    envModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    envModIndicator.setCenterSource (feedbackBase);
    envModIndicator.setDestinationRange (-0.99f, 0.99f);
    envModIndicator.setSource (afEnvLevel, afEnvAmt);

    // Flanger LFO → LFO Start knob: lfoDelayDelta holds (currentDelayMs - delayStart),
    // so raw = delayStart + delta = actual current delay time in ms.
    lfoModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    lfoModIndicator.setModType (LfoModIndicator::ModType::Additive);
    lfoModIndicator.setCenterSource (delayStartBase);
    lfoModIndicator.setDestinationRange (1.0f, 20.0f);
    lfoModIndicator.setSource (lfoDelayDelta, lfoFullDepth);
}

void FlangerPanel::paint (juce::Graphics& g)
{
    // Match the same tint used by all other FX panels.
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    if (omScaled.isValid())
    {
        const int x = omBounds.getX() + (omBounds.getWidth()  - omScaled.getWidth())  / 2;
        const int y = omBounds.getY() + (omBounds.getHeight() - omScaled.getHeight()) / 2;
        g.drawImageAt (omScaled, x, y);
    }

    if (eyeScaled.isValid())
    {
        const int x = eyeBounds.getX() + (eyeBounds.getWidth()  - eyeScaled.getWidth())  / 2;
        const int y = eyeBounds.getY() + (eyeBounds.getHeight() - eyeScaled.getHeight()) / 2;
        g.drawImageAt (eyeScaled, x, y);
    }

}

void FlangerPanel::mouseUp (const juce::MouseEvent& e)
{
    if (omBounds.contains (e.getPosition()) || eyeBounds.contains (e.getPosition()))
        juce::URL ("https://www.facebook.com/groups/162323773830358").launchInDefaultBrowser();
}

void FlangerPanel::mouseMove (const juce::MouseEvent& e)
{
    if (omBounds.contains (e.getPosition()) || eyeBounds.contains (e.getPosition()))
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void FlangerPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromLeft (2);  // 6 px total from left edge

    enableButton.setBounds (area.removeFromLeft (100).withSizeKeepingCentre (100, 24));
    area.removeFromLeft (4);

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

    addKnob (feedbackLabel,   feedbackSlider);
    envModIndicator.setBounds (feedbackSlider.getBounds());
    addKnob (delayStartLabel, delayStartSlider);
    addKnob (delayEndLabel,   delayEndSlider);
    addKnob (rateLabel,       rateSlider);
    lfoModIndicator.setBounds (delayStartSlider.getBounds());
    addKnob (mixLabel,        mixSlider);
    stepModIndicator.setBounds (mixSlider.getBounds());

    // Om image occupies the first of the two previously-empty knob slots after Mix.
    omBounds = area.removeFromLeft (knobW);
    area.removeFromLeft (gap);
    if (omImage.isValid() && omBounds.getWidth() > 0 && omBounds.getHeight() > 0)
    {
        const float scale = juce::jmin ((float) omBounds.getWidth()  / (float) omImage.getWidth(),
                                        (float) omBounds.getHeight() / (float) omImage.getHeight());
        omScaled = omImage.rescaled (juce::roundToInt (omImage.getWidth()  * scale),
                                     juce::roundToInt (omImage.getHeight() * scale),
                                     juce::Graphics::highResamplingQuality);
    }
    // Eye image occupies the second empty knob slot, next to Om.
    eyeBounds = area.removeFromLeft (knobW);
    area.removeFromLeft (gap);
    if (eyeImage.isValid() && eyeBounds.getWidth() > 0 && eyeBounds.getHeight() > 0)
    {
        const float scale = juce::jmin ((float) eyeBounds.getWidth()  / (float) eyeImage.getWidth(),
                                        (float) eyeBounds.getHeight() / (float) eyeImage.getHeight());
        eyeScaled = eyeImage.rescaled (juce::roundToInt (eyeImage.getWidth()  * scale),
                                       juce::roundToInt (eyeImage.getHeight() * scale),
                                       juce::Graphics::highResamplingQuality);
    }
    addKnob (envAmtLabel,     envAmtSlider);
    addKnob (afEnvAmtLabel,   afEnvAmtSlider);
}
