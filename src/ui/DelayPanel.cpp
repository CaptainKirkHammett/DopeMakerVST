#include "DelayPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

namespace
{
    void addDivItems (juce::ComboBox& box)
    {
        const juce::StringArray divNames {
            "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
        };
        for (int i = 0; i < divNames.size(); ++i)
            box.addItem (divNames[i], i + 1);
    }
}

DelayPanel::DelayPanel (juce::AudioProcessorValueTreeState& apvts)
{
    enableButton.getProperties().set ("fxButton", true);
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::delayEnabled, enableButton);

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id) {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("fxKnob", true);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    timeLAttach = makeKnob (timeLSlider, timeLLabel, ParamIDs::delayTimeLMs);
    timeLSlider.getProperties().set ("centreTextKnob", juce::var ("L"));
    timeLLabel .setVisible (false);

    timeRAttach = makeKnob (timeRSlider, timeRLabel, ParamIDs::delayTimeRMs);
    timeRSlider.getProperties().set ("centreTextKnob", juce::var ("R"));
    timeRLabel .setVisible (false);

    fbAttach    = makeKnob (fbSlider,    fbLabel,    ParamIDs::delayFeedback);
    fbSlider   .getProperties().set ("centreTextKnob", juce::var ("FB"));
    fbLabel    .setVisible (false);

    mixAttach   = makeKnob (mixSlider,   mixLabel,   ParamIDs::delayMix);
    mixSlider  .getProperties().set ("centreTextKnob", juce::var ("Mix"));
    mixLabel   .setVisible (false);

    addAndMakeVisible (syncButton);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::delayTempoSync, syncButton);

    duckFader = std::make_unique<DuckFaderButton> (apvts, ParamIDs::delayDuck);
    addAndMakeVisible (*duckFader);

    addDivItems (timeLDivBox);
    addDivItems (timeRDivBox);
    addAndMakeVisible (timeLDivBox);
    addAndMakeVisible (timeRDivBox);
    timeLDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::delayTimeLDiv, timeLDivBox);
    timeRDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::delayTimeRDiv, timeRDivBox);

    // Pulse the parameter 0→1→0 on click so the audio-thread edge detector fires
    // exactly once, and DAW automation can also drive the same parameter without
    // the plugin fighting it with an auto-reset.
    killTailButton.onClick = [&apvts]
    {
        if (auto* p = apvts.getParameter (ParamIDs::delayKillTail))
        {
            p->setValueNotifyingHost (1.0f);
            juce::MessageManager::callAsync ([p] { p->setValueNotifyingHost (0.0f); });
        }
    };
    addAndMakeVisible (killTailButton);

    // Row-2 section group labels.
    for (auto* lbl : { &delayFilterLfoLabel, &delayMixLfoLabel })
    {
        lbl->getProperties().set ("customFont", true);
        lbl->setFont (juce::Font (21.13f, juce::Font::bold));
        addAndMakeVisible (lbl);
    }

    // Feedback filter.
    fbFilterModeBox.addItem ("LP", 1);
    fbFilterModeBox.addItem ("HP", 2);
    fbFilterModeBox.addItem ("BP", 3);
    addAndMakeVisible (fbFilterModeBox);
    fbFilterModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::delayFbFilterMode, fbFilterModeBox);

    fbCutoffAttach = makeKnob (fbCutoffSlider, fbCutoffLabel, ParamIDs::delayFbCutoff);
    fbCutoffSlider.getProperties().set ("centreTextKnob", juce::var ("Cut"));
    fbCutoffLabel .setVisible (false);

    fbResAttach    = makeKnob (fbResSlider,    fbResLabel,    ParamIDs::delayFbResonance);
    fbResSlider   .getProperties().set ("centreTextKnob", juce::var ("Res"));
    fbResLabel    .setVisible (false);

    // Feedback LFO.
    const juce::StringArray fbWaveNames {
        "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Rnd Sine", "Rnd Square"
    };
    for (int i = 0; i < fbWaveNames.size(); ++i)
        fbLfoWaveBox.addItem (fbWaveNames[i], i + 1);
    addAndMakeVisible (fbLfoWaveBox);
    fbLfoWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::delayFbLfoWave, fbLfoWaveBox);

    fbLfoRateAttach  = makeKnob (fbLfoRateSlider,  fbLfoRateLabel,  ParamIDs::delayFbLfoRate);
    fbLfoRateSlider .getProperties().set ("centreTextKnob", juce::var ("Rate"));
    fbLfoRateLabel  .setVisible (false);

    fbLfoDepthAttach = makeKnob (fbLfoDepthSlider, fbLfoDepthLabel, ParamIDs::delayFbLfoDepth);
    fbLfoDepthSlider.getProperties().set ("centreTextKnob", juce::var ("Amt"));
    fbLfoDepthLabel .setVisible (false);

    // Mix LFO.
    addAndMakeVisible (mixLfoSyncButton);
    mixLfoSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::delayMixLfoSync, mixLfoSyncButton);

    addDivItems (mixLfoDivBox);
    addAndMakeVisible (mixLfoDivBox);
    mixLfoDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::delayMixLfoDiv, mixLfoDivBox);

    const juce::StringArray mixWaveNames {
        "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Rnd Sine", "Rnd Square"
    };
    for (int i = 0; i < mixWaveNames.size(); ++i)
        mixLfoWaveBox.addItem (mixWaveNames[i], i + 1);
    addAndMakeVisible (mixLfoWaveBox);
    mixLfoWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::delayMixLfoWave, mixLfoWaveBox);

    mixLfoRateAttach  = makeKnob (mixLfoRateSlider,  mixLfoRateLabel,  ParamIDs::delayMixLfoRate);
    mixLfoRateSlider .getProperties().set ("centreTextKnob", juce::var ("Rate"));
    mixLfoRateLabel  .setVisible (false);

    mixLfoDepthAttach = makeKnob (mixLfoDepthSlider, mixLfoDepthLabel, ParamIDs::delayMixLfoDepth);
    mixLfoDepthSlider.getProperties().set ("centreTextKnob", juce::var ("Amt"));
    mixLfoDepthLabel .setVisible (false);

    envAmtAttach = makeKnob (envAmtSlider, envAmtLabel, ParamIDs::envDelayAmt);
    envAmtSlider.getProperties().set ("fxKnob",   false);
    envAmtSlider.getProperties().set ("gateKnob", true);
    envAmtSlider.getProperties().set ("centreTextKnob", juce::var ("Step"));
    envAmtLabel .setVisible (false);
    stepModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (stepModIndicator);

    mixLfoModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (mixLfoModIndicator);

    fbCutoffModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (fbCutoffModIndicator);

    duckModIndicator.setStyle (LfoModIndicator::Style::RimTip);
    addAndMakeVisible (duckModIndicator);

}

void DelayPanel::setModSources (std::atomic<float>* gateLevel,    std::atomic<float>* gateAmt,
                                 std::atomic<float>* mixBase,
                                 std::atomic<float>* fbLfoOutput,  std::atomic<float>* fbLfoDepth,
                                 std::atomic<float>* fbCutoffBase,
                                 std::atomic<float>* mixLfoOutput, std::atomic<float>* mixLfoDepth,
                                 std::atomic<float>* duckLfoOutput, std::atomic<float>* duckLfoDepth)
{
    // Step/gate → Mix knob.
    stepModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    stepModIndicator.setCenterSource (mixBase);
    stepModIndicator.setDestinationRange (0.0f, 1.0f);
    stepModIndicator.setSource (gateLevel, gateAmt);

    // Delay Filter LFO → FB Cutoff knob (exponential cutoff modulation).
    fbCutoffModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    fbCutoffModIndicator.setModType (LfoModIndicator::ModType::ExponentialOctave);
    fbCutoffModIndicator.setCenterSource (fbCutoffBase);
    fbCutoffModIndicator.setNormalisableRange (
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f));
    fbCutoffModIndicator.setSource (fbLfoOutput, fbLfoDepth);

    // Delay Mix LFO → Mix knob (additive).
    mixLfoModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    mixLfoModIndicator.setCenterSource (mixBase);
    mixLfoModIndicator.setDestinationRange (0.0f, 1.0f);
    mixLfoModIndicator.setSource (mixLfoOutput, mixLfoDepth);

    // Duck → Mix knob: duckLfoOutput is in -1..0 (negated input level),
    // duckLfoDepth is 0.5 when enabled and 0 when disabled (controls visibility).
    // Indicator arc sweeps downward from the knob position as input level rises.
    duckModIndicator.setPointerColour (SuomiLookAndFeel::kModIndicatorFx);
    duckModIndicator.setCenterSource (mixBase);
    duckModIndicator.setDestinationRange (0.0f, 1.0f);
    duckModIndicator.setSource (duckLfoOutput, duckLfoDepth);
}

void DelayPanel::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void DelayPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    area.removeFromLeft (2);  // 6 px total from left edge

    // Row 2 has a 14px title sub-row above its controls.
    // Both control rows are equal height; the panel is sized to keep them
    // the same as they would be with two plain equal rows.
    const int titleH   = 14;
    const int titleGap = 2;
    const int controlH = (area.getHeight() - 4 - titleH - titleGap) / 2;

    auto row1 = area.removeFromTop (controlH);
    area.removeFromTop (4);

    // ── Row 2 section title labels ────────────────────────────────────────────
    // Knob columns reduced 68→44 px so the drawn circle (~36 px) nearly fills
    // the column and the dead-zone gap to neighbouring combos shrinks to ~4 px.
    // Filter LFO: mode(68) + cutoff(45) + res(45) + sep(4) + wave(85) + rate(45) + depth(45)
    // Mix LFO:    sync(98) + div(66) + wave(85) + rate(45) + depth(44)
    constexpr int kFilterLfoW = 68 + 45 + 45 + 4 + 71 + 45 + 45;  // 323  (wave box 68+3)
    constexpr int kMixLfoW    = 98 + 66 + 71 + 45 + 44;            // 324  (wave box 68+3)
    auto titleRow = area.removeFromTop (titleH);
    area.removeFromTop (titleGap);
    auto row2 = area;

    delayFilterLfoLabel.setBounds (titleRow.removeFromLeft (kFilterLfoW));
    titleRow.removeFromLeft (20);
    delayMixLfoLabel.setBounds (titleRow.removeFromLeft (kMixLfoW));

    auto addKnob = [&] (juce::Label& lb, juce::Slider& sl,
                        juce::Rectangle<int>& r, int w) {
        auto col = r.removeFromLeft (w);
        if (!sl.getProperties().contains ("centreTextKnob"))
        {
            lb.setBounds (col.removeFromTop (14));
            lb.setJustificationType (juce::Justification::centred);
        }
        sl.setBounds (col);
        r.removeFromLeft (1);
    };

    // ── Row 1: enable + sync + time params ───────────────────────────────────
    enableButton.setBounds (row1.removeFromLeft (100).withSizeKeepingCentre (100, 24));
    row1.removeFromLeft (8);
    {
        // Sync and Duck share the same 115 px slot the Sync button previously occupied.
        auto syncDuckArea = row1.removeFromLeft (115);
        syncButton.setBounds (syncDuckArea.removeFromLeft (55).withSizeKeepingCentre (55, 24));
        syncDuckArea.removeFromLeft (5);
        duckFader->setBounds (syncDuckArea.withSizeKeepingCentre (55, 24));
    }
    row1.removeFromLeft (8);
    addKnob (timeLLabel, timeLSlider, row1, 44);
    timeLDivBox.setBounds (row1.removeFromLeft (60).withSizeKeepingCentre (60, 24));
    row1.removeFromLeft (2);
    addKnob (timeRLabel, timeRSlider, row1, 44);
    timeRDivBox.setBounds (row1.removeFromLeft (60).withSizeKeepingCentre (60, 24));
    row1.removeFromLeft (4);
    addKnob (fbLabel,     fbSlider,     row1, 44);
    addKnob (mixLabel,    mixSlider,    row1, 44);
    stepModIndicator  .setBounds (mixSlider.getBounds());
    mixLfoModIndicator.setBounds (mixSlider.getBounds());
    duckModIndicator  .setBounds (mixSlider.getBounds());
    addKnob (envAmtLabel, envAmtSlider, row1, 44);
    row1.removeFromLeft (5);
    killTailButton.setBounds (row1.removeFromLeft (75).withSizeKeepingCentre (75, 26));

    // ── Row 2: FB filter + FB LFO + Mix LFO ──────────────────────────────────
    fbFilterModeBox.setBounds (row2.removeFromLeft (65).withSizeKeepingCentre (65, 24));
    row2.removeFromLeft (3);
    addKnob (fbCutoffLabel, fbCutoffSlider, row2, 44);
    addKnob (fbResLabel,    fbResSlider,    row2, 44);
    row2.removeFromLeft (4);
    fbLfoWaveBox.setBounds (row2.removeFromLeft (68).withSizeKeepingCentre (68, 24));
    row2.removeFromLeft (3);
    addKnob (fbLfoRateLabel,  fbLfoRateSlider,  row2, 44);
    addKnob (fbLfoDepthLabel, fbLfoDepthSlider, row2, 44);
    fbCutoffModIndicator.setBounds (fbCutoffSlider.getBounds());
    row2.removeFromLeft (16);
    mixLfoSyncButton.setBounds (row2.removeFromLeft (96).withSizeKeepingCentre (96, 24));
    row2.removeFromLeft (2);
    mixLfoDivBox.setBounds (row2.removeFromLeft (62).withSizeKeepingCentre (62, 24));
    row2.removeFromLeft (4);
    mixLfoWaveBox.setBounds (row2.removeFromLeft (68).withSizeKeepingCentre (68, 24));
    row2.removeFromLeft (3);
    addKnob (mixLfoRateLabel,  mixLfoRateSlider,  row2, 44);
    addKnob (mixLfoDepthLabel, mixLfoDepthSlider, row2, 44);
}
