#include "ArpeggiatorPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

ArpeggiatorPanel::ArpeggiatorPanel (juce::AudioProcessorValueTreeState& apvts)
{
    // Enable button — no fxButton property; matches gate/sidechain section style.
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::arpEnabled, enableButton);

    // Time division combo — sits next to the enable button.
    const juce::StringArray divNames {
        "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
    };
    for (int i = 0; i < divNames.size(); ++i)
        divBox.addItem (divNames[i], i + 1);
    addAndMakeVisible (divBox);
    divAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::arpDivision, divBox);

    // Four small rotary knobs — same style as the sidechain panel.
    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id) {
        sl.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (lb);
        addAndMakeVisible (sl);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    octAttach     = makeKnob (octSlider,     octLabel,     ParamIDs::arpOctaves);
    octSlider.getProperties().set ("centreTextKnob", juce::var ("octave"));

    dirAttach     = makeKnob (dirSlider,     dirLabel,     ParamIDs::arpDirection);
    dirSlider.getProperties().set ("centreTextKnob", juce::var ("dir"));
    noteLenAttach = makeKnob (noteLenSlider, noteLenLabel, ParamIDs::arpNoteLength);
    probAttach    = makeKnob (probSlider,    probLabel,    ParamIDs::arpProbability);
    tieAttach     = makeKnob (tieSlider,     tieLabel,     ParamIDs::arpTie);

    // LFO waveform combo — depth knob controls on/off, no Off item needed.
    for (const auto& name : juce::StringArray { "Sine","Tri","Sqr","SawU","SawD","Rnd~","Rnd|" })
        lfoWaveBox.addItem (name, lfoWaveBox.getNumItems() + 1);
    addAndMakeVisible (lfoWaveBox);
    lfoWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::arpLfoWaveform, lfoWaveBox);

    // LFO division combo (same grid as arp step division).
    for (const auto& name : juce::StringArray { "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1" })
        lfoDivBox.addItem (name, lfoDivBox.getNumItems() + 1);
    addAndMakeVisible (lfoDivBox);
    lfoDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::arpLfoDivision, lfoDivBox);

    // LFO depth knob — same style as the other knobs.
    lfoDepthAttach = makeKnob (lfoDepthSlider, lfoDepthLabel, ParamIDs::arpLfoDepth);

    // Dim all controls when the arpeggiator is disabled.
    // Called on every click and once at construction (after the attachment
    // has restored the saved toggle state) so the initial dim is correct.
    auto applyDim = [this]
    {
        const float a = enableButton.getToggleState() ? 1.0f : 0.35f;
        octSlider    .setAlpha (a);  octLabel    .setAlpha (a);
        dirSlider    .setAlpha (a);  dirLabel    .setAlpha (a);
        noteLenSlider.setAlpha (a);  noteLenLabel.setAlpha (a);
        probSlider   .setAlpha (a);  probLabel   .setAlpha (a);
        tieSlider    .setAlpha (a);  tieLabel    .setAlpha (a);
        divBox          .setAlpha (a);
        lfoWaveBox      .setAlpha (a);
        lfoDivBox       .setAlpha (a);
        lfoDepthSlider  .setAlpha (a);  lfoDepthLabel  .setAlpha (a);
        repaint();
    };
    enableButton.onClick = applyDim;
    applyDim();   // initialise from restored parameter state
}

void ArpeggiatorPanel::paint (juce::Graphics& g)
{
    // Match the gate / sidechain section background.
    // Round bottom corners only — top edge is shared with SidechainPanel.
    g.setColour (SuomiLookAndFeel::kBackground.withAlpha (0.30f));
    {
        juce::Path p;
        auto b = getLocalBounds().toFloat();
        p.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(),
                               6.0f, 6.0f, false, false, true, true);
        g.fillPath (p);
    }
}

void ArpeggiatorPanel::resized()
{
    auto b = getLocalBounds().reduced (5, 0);
    b.setX (2);   // match SidechainPanel origin so buttons align exactly

    constexpr int btnH   = 24;
    constexpr int gap    = 4;
    constexpr int lblH   = 12;

    // ── Top row: enable button + division combo ───────────────────────────────
    // divBox width matches the sidechain envelope viz (right 40% of the panel).
    auto topRow = b.removeFromTop (btnH);
    const int divBoxW = b.getWidth() * 2 / 5;
    divBox.setBounds (topRow.removeFromRight (divBoxW));
    topRow.removeFromRight (gap);
    enableButton.setBounds (topRow);

    b.removeFromTop (gap);

    // ── Bottom area ───────────────────────────────────────────────────────────────
    // Mirror SidechainPanel exactly: removeFromRight(divBoxW) then removeFromRight(4)
    // gives b the same 179px knob-area width, so colW and kGap are identical.
    auto lfoArea = b.removeFromRight (divBoxW);
    b.removeFromRight (4);

    constexpr int kGap  = 3;   // matches SidechainPanel knob spacing
    const int colW   = (b.getWidth() - 4 * kGap) / 5;
    const int knobSz = juce::jmin (colW, b.getHeight() - lblH);

    auto placeKnob = [&] (juce::Label& lb, juce::Slider& sl)
    {
        auto col = b.removeFromLeft (colW);
        lb.setBounds (col.removeFromTop (lblH));
        lb.setJustificationType (juce::Justification::centred);
        sl.setBounds (col.removeFromTop (knobSz).withSizeKeepingCentre (knobSz, knobSz));
        b.removeFromLeft (kGap);
    };

    placeKnob (octLabel,     octSlider);
    placeKnob (dirLabel,     dirSlider);
    placeKnob (noteLenLabel, noteLenSlider);
    placeKnob (probLabel,    probSlider);
    placeKnob (tieLabel,     tieSlider);

    // ── LFO section — laid out within lfoArea ────────────────────────────────
    const int amtColW = colW;
    const int lfoColW = lfoArea.getWidth() - amtColW - 2 * gap;

    auto lfoCol       = lfoArea.removeFromLeft (lfoColW);
    const int comboH2 = (lfoCol.getHeight() - gap) / 2;
    lfoWaveBox.setBounds (lfoCol.removeFromTop (comboH2));
    lfoCol.removeFromTop (gap);
    lfoDivBox .setBounds (lfoCol.removeFromTop (comboH2));

    lfoArea.removeFromLeft (2 * gap);
    auto knobCol = lfoArea.removeFromLeft (amtColW);
    lfoDepthLabel.setBounds (knobCol.removeFromTop (lblH));
    lfoDepthLabel.setJustificationType (juce::Justification::centred);
    lfoDepthSlider.setBounds (knobCol.removeFromTop (knobSz).withSizeKeepingCentre (knobSz, knobSz));
}
