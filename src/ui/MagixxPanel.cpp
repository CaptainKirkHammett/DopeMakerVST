#include "MagixxPanel.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"

MagixxPanel::MagixxPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvts (apvts)
{
    const juce::StringArray waveNames {
        "Sine", "Triangle", "Square", "Saw Up", "Saw Down", "Rnd Sine", "Rnd Square"
    };

    auto makeKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& id)
    {
        sl.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (sl);
        addAndMakeVisible (lb);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, id, sl);
    };

    // ── Start LFO ────────────────────────────────────────────────────────────
    startLfoTitle.getProperties().set ("customFont", true);
    startLfoTitle.setFont (juce::Font (21.13f, juce::Font::bold));
    addAndMakeVisible (startLfoTitle);

    for (int i = 0; i < waveNames.size(); ++i)
        startWaveBox.addItem (waveNames[i], i + 1);
    addAndMakeVisible (startWaveBox);
    startWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::lfoWaveform, startWaveBox);

    addAndMakeVisible (syncButton);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::lfoTempoSync, syncButton);

    {
        syncDivBox.addItem ("1/64", 12);
        const juce::StringArray divNames {
            "1/32", "1/16", "1/8", "1/8D", "1/4", "1/4D",
            "1/2",  "1/2D", "1/1", "1/1D", "2/1"
        };
        for (int i = 0; i < divNames.size(); ++i)
            syncDivBox.addItem (divNames[i], i + 1);
    }
    // Div label not shown per UI spec.
    syncDivLabel.setVisible (false);
    addAndMakeVisible (syncDivBox);
    syncDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::lfoSyncDiv, syncDivBox);

    startRateAttach  = makeKnob (startRateSlider,  startRateLabel,  ParamIDs::lfoRate);
    startDepthAttach = makeKnob (startDepthSlider, startDepthLabel, ParamIDs::lfoDepth);
    startRateSlider .getProperties().set ("thickPointer", true);
    startDepthSlider.getProperties().set ("thickPointer", true);

    // Step override button: step controller drives sample start instead of LFO.
    addAndMakeVisible (stepOverrideButton);
    stepOverrideAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::lfoStartStepOverride, stepOverrideButton);

    // Slicer button: each MIDI note maps to one evenly-spaced slice of the sample window.
    // Mutually exclusive with Step override.
    addAndMakeVisible (slicerButton);
    slicerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::slicerEnabled, slicerButton);

    // Slice1 button: random slice mode — only the Slice Start key triggers, plays a random slice.
    slicerRandomButton.setTooltip (
        "Slice1 - Random Slice Mode\n\n"
        "The Slice Start key picks a random slice each time you press it. "
        "All other slice keys play their assigned slices normally.\n\n"
        "Use this to add unpredictable chops on the trigger key while keeping "
        "regular slice access on the rest of the keyboard.");
    addAndMakeVisible (slicerRandomButton);
    slicerRandomAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::slicerRandomEnabled, slicerRandomButton);

    // Unified visual state update: manages alpha, enabled, and lfoDisabled for
    // both Rate and Depth sliders based on sync, step, and slicer button states.
    // Called from every button/combo callback that can affect the knob state.
    auto updateStartLfoVisuals = [this]
    {
        const bool synced   = syncButton.getToggleState();
        const bool override = stepOverrideButton.getToggleState()
                           || slicerButton.getToggleState()
                           || slicerRandomButton.getToggleState();

        // Rate is disabled by sync OR any override mode.
        const bool rateOff  = synced || override;
        const float rateA   = rateOff ? 0.35f : 1.0f;
        startRateSlider.setAlpha   (rateA);
        startRateLabel .setAlpha   (rateA);
        startRateSlider.setEnabled (! rateOff);
        startRateSlider.getProperties().set ("lfoDisabled", rateOff);
        startRateSlider.repaint();

        // Depth is disabled only by override modes (sync doesn't affect depth).
        const float depthA = override ? 0.35f : 1.0f;
        startDepthSlider.setAlpha   (depthA);
        startDepthLabel .setAlpha   (depthA);
        startDepthSlider.setEnabled (! override);
        startDepthSlider.getProperties().set ("lfoDisabled", override);
        startDepthSlider.repaint();

        // Sync division box.
        const float divA = synced ? 1.0f : 0.35f;
        syncDivBox.setAlpha   (divA);
        syncDivBox.setEnabled (synced);
    };

    // Mutual exclusion: Step is exclusive with Slicer/Slice1.
    // Slice1 is a sub-mode of Slicer — both stay on together.
    stepOverrideButton.onStateChange = [this, updateStartLfoVisuals]
    {
        if (stepOverrideButton.getToggleState())
        {
            slicerButton.setToggleState (false, juce::sendNotification);
            slicerRandomButton.setToggleState (false, juce::sendNotification);
            if (this->apvts.getRawParameterValue (ParamIDs::gateEnabled)->load() < 0.5f)
                if (auto* p = this->apvts.getParameter (ParamIDs::gateEnabled))
                    p->setValueNotifyingHost (1.0f);
        }
        updateStartLfoVisuals();
    };
    slicerButton.onStateChange = [this, updateStartLfoVisuals]
    {
        if (slicerButton.getToggleState())
        {
            stepOverrideButton.setToggleState (false, juce::sendNotification);
        }
        else
        {
            // Turning Slicer off also turns off Slice1 (it depends on Slicer).
            slicerRandomButton.setToggleState (false, juce::sendNotification);
        }
        updateStartLfoVisuals();
    };
    slicerRandomButton.onStateChange = [this, updateStartLfoVisuals]
    {
        if (slicerRandomButton.getToggleState())
        {
            stepOverrideButton.setToggleState (false, juce::sendNotification);
            if (! slicerButton.getToggleState())
                slicerButton.setToggleState (true, juce::sendNotification);
        }
        updateStartLfoVisuals();
    };
    syncButton.onStateChange = updateStartLfoVisuals;

    // Re-evaluate knob state when waveform changes — the ComboBox popup's
    // modal cycle can leave the slider's enabled state stale on some hosts.
    startWaveBox.onChange = [updateStartLfoVisuals] { updateStartLfoVisuals(); };

    updateStartLfoVisuals();   // apply immediately (e.g. after project restore)

    // ── Velocity LFO ─────────────────────────────────────────────────────────
    velLfoTitle.getProperties().set ("customFont", true);
    velLfoTitle.setFont (juce::Font (21.13f, juce::Font::bold));
    addAndMakeVisible (velLfoTitle);

    for (int i = 0; i < waveNames.size(); ++i)
        velWaveBox.addItem (waveNames[i], i + 1);
    addAndMakeVisible (velWaveBox);
    velWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::velLfoWaveform, velWaveBox);

    addAndMakeVisible (velSyncButton);
    velSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::velLfoTempoSync, velSyncButton);

    {
        velSyncDivBox.addItem ("1/64", 12);
        const juce::StringArray divNames {
            "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
        };
        for (int i = 0; i < divNames.size(); ++i)
            velSyncDivBox.addItem (divNames[i], i + 1);
    }
    velSyncDivLabel.setVisible (false);
    addAndMakeVisible (velSyncDivBox);
    velSyncDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::velLfoSyncDiv, velSyncDivBox);

    velRateAttach  = makeKnob (velRateSlider,  velRateLabel,  ParamIDs::velLfoRate);
    velDepthAttach = makeKnob (velDepthSlider, velDepthLabel, ParamIDs::velLfoDepth);

    // Grey out Velocity Rate knob when Sync is active.
    auto velSyncVisual = [this]
    {
        const bool synced = velSyncButton.getToggleState();
        const float a = synced ? 0.35f : 1.0f;
        velRateSlider.setAlpha (a);
        velRateLabel .setAlpha (a);
        velRateSlider.setEnabled (! synced);
        velRateSlider.repaint();

        const float divA = synced ? 1.0f : 0.35f;
        velSyncDivBox.setAlpha (divA);
        velSyncDivBox.setEnabled (synced);
    };
    velSyncButton.onStateChange = velSyncVisual;
    velWaveBox.onChange = [velSyncVisual] { velSyncVisual(); };
    velSyncVisual();

    addAndMakeVisible (velMeter);

    // Probability knob — styled as a standard FX knob (smaller than Magixx).
    velProbSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    velProbSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (velProbLabel);
    addAndMakeVisible (velProbSlider);
    velProbAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::velLfoProb, velProbSlider);

    // ── Rel LFO ───────────────────────────────────────────────────────────────
    relLfoTitle.getProperties().set ("customFont", true);
    relLfoTitle.setFont (juce::Font (21.13f, juce::Font::bold));
    addAndMakeVisible (relLfoTitle);

    for (int i = 0; i < waveNames.size(); ++i)
        relWaveBox.addItem (waveNames[i], i + 1);
    addAndMakeVisible (relWaveBox);
    relWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::volRelLfoWaveform, relWaveBox);

    addAndMakeVisible (relSyncButton);
    relSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::volRelLfoTempoSync, relSyncButton);

    {
        relSyncDivBox.addItem ("1/64", 12);
        const juce::StringArray divNames {
            "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
        };
        for (int i = 0; i < divNames.size(); ++i)
            relSyncDivBox.addItem (divNames[i], i + 1);
    }
    // Div label not shown per UI spec.
    relSyncDivLabel.setVisible (false);
    addAndMakeVisible (relSyncDivBox);
    relSyncDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::volRelLfoSyncDiv, relSyncDivBox);

    relRateAttach  = makeKnob (relRateSlider,  relRateLabel,  ParamIDs::volRelLfoRate);
    relDepthAttach = makeKnob (relDepthSlider, relDepthLabel, ParamIDs::volRelLfoDepth);

    // Grey out Release Rate knob when Sync is active.
    auto relSyncVisual = [this]
    {
        const bool synced = relSyncButton.getToggleState();
        const float a = synced ? 0.35f : 1.0f;
        relRateSlider.setAlpha (a);
        relRateLabel .setAlpha (a);
        relRateSlider.setEnabled (! synced);
        relRateSlider.repaint();

        const float divA = synced ? 1.0f : 0.35f;
        relSyncDivBox.setAlpha (divA);
        relSyncDivBox.setEnabled (synced);
    };
    relSyncButton.onStateChange = relSyncVisual;
    relWaveBox.onChange = [relSyncVisual] { relSyncVisual(); };
    relSyncVisual();

    // ── Pitch LFO ─────────────────────────────────────────────────────────────
    pitchLfoTitle.getProperties().set ("customFont", true);
    pitchLfoTitle.setFont (juce::Font (21.13f, juce::Font::bold));
    addAndMakeVisible (pitchLfoTitle);

    for (int i = 0; i < waveNames.size(); ++i)
        pitchWaveBox.addItem (waveNames[i], i + 1);
    addAndMakeVisible (pitchWaveBox);
    pitchWaveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::pitchLfoWaveform, pitchWaveBox);

    addAndMakeVisible (pitchSyncButton);
    pitchSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::pitchLfoTempoSync, pitchSyncButton);

    {
        pitchSyncDivBox.addItem ("1/64", 12);
        const juce::StringArray divNames {
            "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
        };
        for (int i = 0; i < divNames.size(); ++i)
            pitchSyncDivBox.addItem (divNames[i], i + 1);
    }
    pitchSyncDivLabel.setVisible (false);
    addAndMakeVisible (pitchSyncDivBox);
    pitchSyncDivAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::pitchLfoSyncDiv, pitchSyncDivBox);

    pitchRateAttach  = makeKnob (pitchRateSlider,  pitchRateLabel,  ParamIDs::pitchLfoRate);
    pitchDepthAttach = makeKnob (pitchDepthSlider, pitchDepthLabel, ParamIDs::pitchLfoDepth);

    // Grey out Pitch Rate knob when Sync is active.
    auto pitchSyncVisual = [this]
    {
        const bool synced = pitchSyncButton.getToggleState();
        const float a = synced ? 0.35f : 1.0f;
        pitchRateSlider.setAlpha (a);
        pitchRateLabel .setAlpha (a);
        pitchRateSlider.setEnabled (! synced);
        pitchRateSlider.repaint();

        const float divA = synced ? 1.0f : 0.35f;
        pitchSyncDivBox.setAlpha (divA);
        pitchSyncDivBox.setEnabled (synced);
    };
    pitchSyncButton.onStateChange = pitchSyncVisual;
    pitchWaveBox.onChange = [pitchSyncVisual] { pitchSyncVisual(); };
    pitchSyncVisual();
}

void MagixxPanel::paint (juce::Graphics& g)
{
    // Overall panel background — same blue hue as the main UI background.
    g.setColour (SuomiLookAndFeel::kBackground.withAlpha (0.30f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // Embossed raised shape behind each LFO column.
    for (const auto& r : columnBounds)
    {
        if (r.isEmpty()) continue;

        const auto rf = r.toFloat();
        constexpr float corner = 6.0f;

        // Drop shadow — dark, shifted down-right.
        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.fillRoundedRectangle (rf.translated (2.0f, 2.0f), corner);

        // Body — blue hue matching the main UI background, more opaque for depth.
        g.setColour (SuomiLookAndFeel::kBackground.withAlpha (0.60f));
        g.fillRoundedRectangle (rf, corner);

        // Top highlight edge (raised feel).
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (rf.withHeight (corner), corner);

    }
}

void MagixxPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft   (14);
    area.removeFromRight  (14);
    area.removeFromTop    (4);
    area.removeFromBottom (14);

    constexpr int kProbW  = 45;
    constexpr int lblH    = 12;
    constexpr int rowH    = 22;
    constexpr int gap     = 4;

    // Knob size scales with panel height so the layout stays sensible whether the
    // panel is at 168 px (full size, kW=84) or 120 px (compact, kW≈36).
    // Overhead per column: title(12) + gap(4) + waveRow(22) + gap(4) + label(12) = 54 px.
    const int innerH = area.getHeight() - 12;  // minus reduced(8,6) top+bottom
    const int kW     = juce::jmax (24, juce::jmin (84, innerH - 54));

    granulizerSlotBounds = {};  // not used in this layout

    // Four-column layout: Start LFO | Vol Rel LFO | Pitch LFO | Velocity LFO, 16px gaps.
    const int colW = (area.getWidth() - 48) / 4;
    auto col1 = area.removeFromLeft (colW);  area.removeFromLeft (16);
    auto col2 = area.removeFromLeft (colW);  area.removeFromLeft (16);
    auto col3 = area.removeFromLeft (colW);  area.removeFromLeft (16);
    auto col4 = area;

    columnBounds = { col1.expanded (2), col2.expanded (2),
                     col3.expanded (2), col4.expanded (2) };
    repaint();

    auto inner1 = col1.reduced (8, 6);
    auto inner2 = col2.reduced (8, 6);
    auto inner3 = col3.reduced (8, 6);
    auto inner4 = col4.reduced (8, 6);

    auto layoutWaveRow = [&] (juce::Rectangle<int>& col,
                              juce::ComboBox& waveBox,
                              juce::ToggleButton& syncBtn, juce::ComboBox& divBox)
    {
        auto row = col.removeFromTop (rowH);
        const int waveBoxW = juce::jmax (50, row.getWidth() - 106);
        waveBox .setBounds (row.removeFromLeft (waveBoxW));
        row.removeFromLeft (gap);
        syncBtn .setBounds (row.removeFromLeft (40));
        row.removeFromLeft (gap);
        divBox  .setBounds (row.removeFromLeft (58));
    };

    auto layoutLfoColumn = [&] (juce::Rectangle<int>& col,
                                juce::Label& title,
                                juce::ComboBox& waveBox,
                                juce::ToggleButton& syncBtn, juce::ComboBox& divBox,
                                juce::Label& rateLabel,  juce::Slider& rateSlider,
                                juce::Label& depthLabel, juce::Slider& depthSlider)
    {
        title.setBounds (col.removeFromTop (lblH));
        col.removeFromTop (gap);
        layoutWaveRow (col, waveBox, syncBtn, divBox);
        col.removeFromTop (gap);

        auto rateCol  = col.removeFromLeft (kW);
        col.removeFromLeft (gap * 2);
        auto depthCol = col.removeFromLeft (kW);

        rateLabel  .setBounds (rateCol .removeFromTop (lblH));
        rateSlider .setBounds (rateCol .removeFromTop (kW).withWidth (kW));
        depthLabel .setBounds (depthCol.removeFromTop (lblH));
        depthSlider.setBounds (depthCol.removeFromTop (kW).withWidth (kW));
    };

    auto layoutVelocityColumn = [&] (juce::Rectangle<int>& col)
    {
        velLfoTitle.setBounds (col.removeFromTop (lblH));
        col.removeFromTop (gap);
        layoutWaveRow (col, velWaveBox, velSyncButton, velSyncDivBox);
        col.removeFromTop (gap);

        auto rateCol  = col.removeFromLeft (kW);
        col.removeFromLeft (gap * 2);
        auto depthCol = col.removeFromLeft (kW);
        col.removeFromLeft (gap * 2);
        const int probW = juce::jmin (kProbW, kW);
        auto probCol  = col.removeFromLeft (probW);

        velRateLabel .setBounds  (rateCol .removeFromTop (lblH));
        velRateSlider.setBounds  (rateCol .removeFromTop (kW).withWidth (kW));
        velDepthLabel.setBounds  (depthCol.removeFromTop (lblH));
        velDepthSlider.setBounds (depthCol.removeFromTop (kW).withWidth (kW));
        velProbLabel .setBounds  (probCol .removeFromTop (lblH));
        velProbLabel .setJustificationType (juce::Justification::centred);
        velProbSlider.setBounds  (probCol .removeFromTop (probW).withWidth (probW));

        probCol.removeFromTop (gap);
        const int meterW = probW / 4;
        velMeter.setBounds (probCol.withSizeKeepingCentre (meterW, probCol.getHeight()));
    };

    // Start LFO — inline so Step + Slicer override buttons fit on the knob row.
    {
        auto& col = inner1;
        startLfoTitle.setBounds (col.removeFromTop (lblH));
        col.removeFromTop (gap);
        layoutWaveRow (col, startWaveBox, syncButton, syncDivBox);
        col.removeFromTop (gap);

        const int knobRowY = col.getY();
        auto rateCol  = col.removeFromLeft (kW);
        col.removeFromLeft (gap * 2);
        auto depthCol = col.removeFromLeft (kW);
        col.removeFromLeft (gap * 2);

        startRateLabel  .setBounds (rateCol .removeFromTop (lblH));
        startRateSlider .setBounds (rateCol .removeFromTop (kW).withWidth (kW));
        startDepthLabel .setBounds (depthCol.removeFromTop (lblH));
        startDepthSlider.setBounds (depthCol.removeFromTop (kW).withWidth (kW));

        constexpr int btnH   = 22;
        constexpr int btnGap = 2;
        const int stackH  = btnH * 3 + btnGap * 2;
        const int btnX    = col.getX();
        const int btnW    = juce::jmax (38, col.getWidth());
        const int btnTopY = knobRowY + lblH + (kW - stackH) / 2;
        stepOverrideButton.setBounds (btnX, btnTopY,                          btnW, btnH);
        slicerButton      .setBounds (btnX, btnTopY + btnH + btnGap,          btnW, btnH);
        slicerRandomButton.setBounds (btnX, btnTopY + (btnH + btnGap) * 2,    btnW, btnH);
    }

    layoutLfoColumn (inner2, relLfoTitle,
                     relWaveBox,   relSyncButton,   relSyncDivBox,
                     relRateLabel,   relRateSlider,  relDepthLabel,  relDepthSlider);

    layoutLfoColumn (inner3, pitchLfoTitle,
                     pitchWaveBox, pitchSyncButton, pitchSyncDivBox,
                     pitchRateLabel, pitchRateSlider, pitchDepthLabel, pitchDepthSlider);

    layoutVelocityColumn (inner4);
}

void MagixxPanel::setVelocitySource (std::atomic<float>*    lastVelocity,
                                     std::atomic<uint32_t>* generation)
{
    velMeter.setSource (lastVelocity, generation);
}
