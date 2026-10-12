#include "PluginEditor.h"
#include "ParameterIDs.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      paramDisplay       (p.apvts),
      waveformDisplay    (p.apvts, p.formatManager, p.thumbnailCache),
      sampleControlPanel (p.apvts),
      volEnvPanel        (p.apvts, "vol",   false),
      pitchEnvPanel      (p.apvts, "pitch", true),
      magixxPanel        (p.apvts),
      granulizerPanel     (p.apvts),
      stutterPanel        (p.apvts),
      gateControllerPanel (p.apvts, p.gateController),
      sidechainPanel      (p.apvts),
      arpPanel            (p.apvts),
      filterPanel        (p.apvts),
      envFollowerPanel   (p.apvts),
      distPanel          (p.apvts),
      flangerPanel       (p.apvts),
      reverbPanel        (p.apvts),
      phaserPanel        (p.apvts),
      highcutPanel       (p.apvts),
      delayPanel         (p.apvts),
      lowcutPanel        (p.apvts),
      clipPanel          (p.apvts),
      suppressorPanel    (p.apvts),
      presetManagerPanel (p.presetManager)
{
    // Apply custom LookAndFeel to this editor and all its children.
    setLookAndFeel (&suomiLAF);

    // Disable drag-resize — size is controlled only via the scale selector.
    setResizable (false, false);

    // uiRoot is always 1050 × 980.  A scale transform is applied to it in
    // resized() so that all child components scale uniformly without any
    // individual layout code needing to know about the current scale factor.
    uiRoot.setBounds (0, 0, 1050, 1108);
    addAndMakeVisible (uiRoot);

    setSize (1050, 1108);

    // Cover art added first so it paints behind all other children.
    uiRoot.addAndMakeVisible (coverArt);

    // Header.
    titleLabel.getProperties().set ("customFont", true);
    titleLabel.setText ("Dope Maker VST", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (93.31f, juce::Font::bold));  // 15 % smaller than original 109.78 f
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcc88ff));
    titleLabel.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    titleLabel.setTooltip ("Signal chain map");
    titleLabel.addMouseListener (this, false);
    uiRoot.addAndMakeVisible (titleLabel);

    betaLabel.setText ("Beta v0.2780.0.4", juce::dontSendNotification);
    betaLabel.setFont (juce::Font (16.0f, juce::Font::italic));
    betaLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcc88ff).withAlpha (0.7f));
    betaLabel.setJustificationType (juce::Justification::topLeft);
    uiRoot.addAndMakeVisible (betaLabel);

    logoButton.setTooltip ("Enter The DOPE TOWN");
    logoButton.addMouseListener (this, false);
    uiRoot.addAndMakeVisible (logoButton);

    contactLabel.setText ("Contact", juce::dontSendNotification);
    contactLabel.setFont (juce::Font (16.0f, juce::Font::italic));
    contactLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcc88ff).withAlpha (0.7f));
    contactLabel.setJustificationType (juce::Justification::centredRight);
    contactLabel.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    contactLabel.setTooltip ("Send EMail");
    contactLabel.addMouseListener (this, false);
    uiRoot.addAndMakeVisible (contactLabel);

    donateLabel.setText ("Donate", juce::dontSendNotification);
    donateLabel.setFont (juce::Font (16.0f, juce::Font::italic));
    donateLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcc88ff).withAlpha (0.7f));
    donateLabel.setJustificationType (juce::Justification::centredRight);
    donateLabel.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    donateLabel.setTooltip ("If you find this VST fun, you could always Paypal me few bucks RIGHT!?");
    donateLabel.addMouseListener (this, false);
    uiRoot.addAndMakeVisible (donateLabel);

    bqButton.getProperties().set ("fxButton", true);
    bqButton.setTooltip ("Better Quality Interpolation\n\n"
                         "OFF  Good Quality (Hermite cubic): smooth 4-point interpolation that works well "
                         "for most pitch shifts. Very low CPU cost.\n\n"
                         "ON   Better Quality (windowed sinc): 8-point sinc interpolation with Blackman-Harris "
                         "window. Virtually eliminates aliasing artifacts at extreme pitch shifts (octave+). "
                         "Higher CPU cost per voice - use when sound quality matters more than polyphony headroom.");
    bqAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::hqInterpolation, bqButton);
    uiRoot.addAndMakeVisible (bqButton);

    zcButton.getProperties().set ("fxButton", true);
    zcButton.setTooltip ("ZC Mode\n\n"
                         "OFF  Shared zero-crossing: both channels snap to the same (earliest) zero-crossing "
                         "frame, keeping L and R perfectly in sync. Recommended for most samples.\n\n"
                         "ON   Per-channel zero-crossing: each channel independently finds its own nearest "
                         "zero-crossing at the start and end boundaries. Tighter click suppression on stereo "
                         "samples with very different L/R waveforms, but may introduce a small timing offset "
                         "between channels. Spugedelic Stereo Mode.");
    zcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, ParamIDs::zcPerChannel, zcButton);
    uiRoot.addAndMakeVisible (zcButton);

    loadButton.onClick = [this] { onLoadButtonClicked(); };
    uiRoot.addAndMakeVisible (loadButton);

    openFolderButton.setTooltip ("Open presets folder");
    openFolderButton.onClick = [this]
    {
        auto folder = PresetManager::getPresetsRoot();
        folder.createDirectory();
        folder.revealToUser();
    };
    uiRoot.addAndMakeVisible (openFolderButton);

    defaultButton.onClick = [this] { onDefaultButtonClicked(); };
    uiRoot.addAndMakeVisible (defaultButton);

    presetButton.onClick = [this]
    {
        if (presetManagerPanel.isVisible())
        {
            presetManagerPanel.hide();
        }
        else
        {
            // Panel anchors just below the header buttons, aligned to the right.
            const auto btnBounds = presetButton.getBoundsInParent();
            const int  panelW    = 480;
            const int  panelH    = 380;
            const int  panelX    = juce::jmax (0, btnBounds.getRight() - panelW);
            const int  panelY    = btnBounds.getBottom() + 4;
            presetManagerPanel.setCurrentSampleFile (processor.getLastLoadedFile());
            presetManagerPanel.show ({ panelX, panelY, panelW, panelH });
        }
    };
    uiRoot.addAndMakeVisible (presetButton);

    prevPresetButton.onClick = [this] { presetManagerPanel.navigateBy (-1); };
    nextPresetButton.onClick = [this] { presetManagerPanel.navigateBy ( 1); };
    uiRoot.addAndMakeVisible (prevPresetButton);
    uiRoot.addAndMakeVisible (nextPresetButton);

    // Wire up the panel so loading a preset also refreshes the waveform.
    presetManagerPanel.onPresetLoaded = [this] (const juce::File& sampleFile)
    {
        // Request a DSP flush so no audio from the previous preset spills over
        // (delay tails, reverb tails, stutter capture buffer, etc.).
        // The actual flush runs on the audio thread at the top of processBlock().
        processor.requestDspFlush();

        if (sampleFile.existsAsFile())
        {
            processor.loadSampleFile (sampleFile);
            waveformDisplay.setFile (sampleFile);
            waveformDisplay.setPresetName (processor.presetManager.getCurrentPresetName());
            presetManagerPanel.setCurrentSampleFile (sampleFile);
        }
    };
    uiRoot.addChildComponent (presetManagerPanel);

    // Waveform.
    waveformDisplay.setEngine (processor.getEngine());
    waveformDisplay.setGranulizer (&processor.granulizerProc);
    waveformDisplay.setNormalizeGainSource (&processor.normalizeGain);
    sampleControlPanel.setLimiterGainReductionSource (&processor.limiterGainReductionDb);
    waveformDisplay.onFileDrop = [this] (const juce::File& f) {
        processor.loadSampleFile (f);
        waveformDisplay.setFile (f);
        presetManagerPanel.setCurrentSampleFile (f);
    };
    uiRoot.addAndMakeVisible (waveformDisplay);

    // If a sample was already loaded (e.g. project restored via setStateInformation
    // before this editor was constructed), rebuild the thumbnail now.
    if (const auto& f = processor.getLastLoadedFile(); f.existsAsFile())
    {
        waveformDisplay.setFile (f);
        waveformDisplay.setPresetName (processor.presetManager.getCurrentPresetName());
    }
    else if (processor.sampleMissing)
        waveformDisplay.setMissingSample (processor.missingSampleName);

    sampleControlPanel.setStereoFieldSource (&processor.stereoFieldSource);
    uiRoot.addAndMakeVisible (sampleControlPanel);
    sampleControlPanel.setLfoModSource (&p.getEngine()->pitchLfoOutput,
                                        p.apvts.getRawParameterValue (ParamIDs::pitchLfoDepth));
    uiRoot.addAndMakeVisible (volEnvPanel);
    volEnvPanel.setReleaseModSource (&p.getEngine()->volRelLfoOutput,
                                     p.apvts.getRawParameterValue (ParamIDs::volRelLfoDepth),
                                     p.apvts.getRawParameterValue (ParamIDs::volRelease));
    uiRoot.addAndMakeVisible (pitchEnvPanel);
    uiRoot.addAndMakeVisible (magixxPanel);
    magixxPanel.setVelocitySource (&p.getEngine()->lastPlayedVelocity,
                                   &p.getEngine()->noteOnGeneration);
    uiRoot.addAndMakeVisible (granulizerPanel);
    granulizerPanel.setModSources (&p.granulizerProc.lfoOutput,
                                   p.apvts.getRawParameterValue (ParamIDs::granDepth),
                                   p.apvts.getRawParameterValue (ParamIDs::granWaveSpacing));
    uiRoot.addAndMakeVisible (stutterPanel);
    stutterPanel.setEffectiveDivSource (&p.stutterProc.currentEffectiveDivIndex);
    stutterPanel.setLenLfoModSource (&p.stutterLenLfoOutput,
                                      p.apvts.getRawParameterValue (ParamIDs::stutterLenLfoDepth));
    uiRoot.addAndMakeVisible (gateControllerPanel);
    gateControllerPanel.setParameterDisplay (&paramDisplay);
    uiRoot.addAndMakeVisible (sidechainPanel);
    uiRoot.addAndMakeVisible (arpPanel);
    uiRoot.addAndMakeVisible (filterPanel);
    filterPanel.setModSources (&p.gateController.currentStepLevelForUI,
                                p.apvts.getRawParameterValue (ParamIDs::envFilterAmt),
                                &p.audioEnvLevelForUI,
                                p.apvts.getRawParameterValue (ParamIDs::afEnvFilterAmt),
                                p.apvts.getRawParameterValue (ParamIDs::filterCutoff),
                                &p.filterLfoOutput,
                                p.apvts.getRawParameterValue (ParamIDs::filterLfoDepth));
    uiRoot.addAndMakeVisible (envFollowerPanel);
    uiRoot.addAndMakeVisible (distPanel);
    distPanel.setModSources  (&p.gateController.currentStepLevelForUI,
                                p.apvts.getRawParameterValue (ParamIDs::envDistAmt),
                                &p.audioEnvLevelForUI,
                                p.apvts.getRawParameterValue (ParamIDs::afEnvDistAmt),
                                p.apvts.getRawParameterValue (ParamIDs::distDrive));
    uiRoot.addAndMakeVisible (flangerPanel);
    flangerPanel.setModSources (&p.gateController.currentStepLevelForUI,
                                 p.apvts.getRawParameterValue (ParamIDs::envFlangerAmt),
                                 &p.audioEnvLevelForUI,
                                 p.apvts.getRawParameterValue (ParamIDs::afEnvFlangerAmt),
                                 p.apvts.getRawParameterValue (ParamIDs::flangerMix),
                                 p.apvts.getRawParameterValue (ParamIDs::flangerFeedback),
                                 &p.flangerProc.lfoDelayDelta,
                                 &p.flangerProc.lfoFullDepth,
                                 p.apvts.getRawParameterValue (ParamIDs::flangerDelayStart));
    uiRoot.addAndMakeVisible (reverbPanel);
    reverbPanel.setDuckModSources (p.apvts.getRawParameterValue (ParamIDs::reverbMix),
                                   &p.reverbProc.duckLfoOutput,
                                   &p.reverbProc.duckLfoDepth);
    uiRoot.addAndMakeVisible (phaserPanel);
    phaserPanel.setModSources  (&p.gateController.currentStepLevelForUI,
                                 p.apvts.getRawParameterValue (ParamIDs::envPhaserAmt),
                                 &p.audioEnvLevelForUI,
                                 p.apvts.getRawParameterValue (ParamIDs::afEnvPhaserAmt),
                                 p.apvts.getRawParameterValue (ParamIDs::phaserMix),
                                 p.apvts.getRawParameterValue (ParamIDs::phaserFeedback),
                                 &p.phaserProc.lfoOutput,
                                 p.apvts.getRawParameterValue (ParamIDs::phaserDepth),
                                 p.apvts.getRawParameterValue (ParamIDs::phaserCentreFreq));
    highcutPanel.setSpectrumSource (&processor.lowcutSpectrumSource);
    uiRoot.addAndMakeVisible (highcutPanel);
    uiRoot.addAndMakeVisible (delayPanel);
    delayPanel.setModSources   (&p.gateController.currentStepLevelForUI,
                                 p.apvts.getRawParameterValue (ParamIDs::envDelayAmt),
                                 p.apvts.getRawParameterValue (ParamIDs::delayMix),
                                 &p.delayProc.fbLfoOutput,
                                 p.apvts.getRawParameterValue (ParamIDs::delayFbLfoDepth),
                                 p.apvts.getRawParameterValue (ParamIDs::delayFbCutoff),
                                 &p.delayProc.mixLfoOutput,
                                 p.apvts.getRawParameterValue (ParamIDs::delayMixLfoDepth),
                                 &p.delayProc.duckLfoOutput,
                                 &p.delayProc.duckLfoDepth);
    lowcutPanel.setSpectrumSource (&processor.lowcutSpectrumSource);
    uiRoot.addAndMakeVisible (lowcutPanel);
    uiRoot.addAndMakeVisible (clipPanel);

    uiRoot.addAndMakeVisible (suppressorPanel);

    // Parameter display — shows name + value of last tweaked control.
    uiRoot.addAndMakeVisible (paramDisplay);

    // Scale selector — 100 % / 75 % / 50 %; changes plugin window size.
    // resized() applies the matching transform to uiRoot automatically.
    scaleBox.getProperties().set ("centredText", true);
    scaleBox.addItem ("100%", 1);
    scaleBox.addItem ("85%",  2);
    scaleBox.addItem ("75%",  3);
    scaleBox.addItem ("60%",  4);
    scaleBox.addItem ("50%",  5);
    scaleBox.setTextWhenNothingSelected ("UI Scale");
    scaleBox.onChange = [this]
    {
        const float scales[] = { 1.0f, 0.85f, 0.75f, 0.60f, 0.5f };
        const float s = scales[juce::jlimit (0, 4, scaleBox.getSelectedId() - 1)];
        setSize (juce::roundToInt (1050.0f * s), juce::roundToInt (1108.0f * s));
        scaleBox.setSelectedId (0, juce::dontSendNotification);
    };
    uiRoot.addAndMakeVisible (scaleBox);

    // Register as APVTS listener for all parameters so paramDisplay updates
    // whenever the user moves any control.
    for (auto* param : p.getParameters())
        if (auto* rap = dynamic_cast<juce::RangedAudioParameter*> (param))
            p.apvts.addParameterListener (rap->getParameterID(), this);

    // Pre/Post gain knobs — small rotary knobs overlaid on VU meters.
    auto setupGainKnob = [&] (juce::Slider& sl, juce::Label& lb, const juce::String& paramId)
    {
        sl.setSliderStyle  (juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        sl.getProperties().set ("gainKnob", true);
        lb.setVisible (false);
        return std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            p.apvts, paramId, sl);
    };
    preGainAttach  = setupGainKnob (preGainSlider,  preGainLabel,  ParamIDs::preGainDb);
    postGainAttach = setupGainKnob (postGainSlider, postGainLabel, ParamIDs::postGainDb);

    // VU meters read the processor's atomic level feeds.
    preFxMeter .setLevelSources (&processor.vuPreFxL,  &processor.vuPreFxR);
    postFxMeter.setLevelSources (&processor.vuPostFxL, &processor.vuPostFxR);

    // Clip overlay on the post meter — shows ceiling and active clipping.
    postFxMeter.setClipSources (&processor.clipPrePeakL,
                                &processor.clipPrePeakR,
                                &processor.clipThreshold,
                                &processor.clipModeState);

    uiRoot.addAndMakeVisible (preFxMeter);
    uiRoot.addAndMakeVisible (postFxMeter);

    // Gain knobs added after VU meters so they paint on top.
    uiRoot.addAndMakeVisible (preGainSlider);
    uiRoot.addAndMakeVisible (postGainSlider);

    // Spuge overlay added last so it paints above all panels while passing mouse events through.
    uiRoot.addAndMakeVisible (spugeOverlay);

    // Transparent click area over the cover art image — added last so it sits on top of panels.
    coverArtClickArea.setAlpha (0.0f);
    coverArtClickArea.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    coverArtClickArea.onClick = [] {
        juce::URL ("https://dopescience.bandcamp.com/").launchInDefaultBrowser();
    };
    uiRoot.addAndMakeVisible (coverArtClickArea);

    uiRoot.addChildComponent (signalPathOverlay);

    // One-shot timer: catches DAWs that call setStateInformation after
    // createEditor, so the preset name and sample show up even then.
    startTimer (200);
}

PluginEditor::~PluginEditor()
{
    stopTimer();

    for (auto* param : processor.getParameters())
        if (auto* rap = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor.apvts.removeParameterListener (rap->getParameterID(), this);

    setLookAndFeel (nullptr);
}

void PluginEditor::timerCallback()
{
    stopTimer();  // one-shot

    // Re-read preset name and sample from the processor in case
    // setStateInformation was called after createEditor.
    if (const auto& f = processor.getLastLoadedFile(); f.existsAsFile())
    {
        waveformDisplay.setFile (f);

        const auto& name = processor.presetManager.getCurrentPresetName();
        if (name.isNotEmpty())
            waveformDisplay.setPresetName (name);
    }
    else if (processor.sampleMissing)
    {
        waveformDisplay.setMissingSample (processor.missingSampleName);
    }
}

void PluginEditor::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent == &contactLabel)
    {
        // Built from parts at runtime to avoid a plain-text email in the binary.
        juce::String addr;
        addr << "ulle" << "." << "kir" << "@" << "gmail" << "." << "com";
        juce::URL ("mailto:" + addr).launchInDefaultBrowser();
    }
    else if (e.eventComponent == &titleLabel)
    {
        signalPathOverlay.setBounds (uiRoot.getLocalBounds());
        signalPathOverlay.setVisible (true);
        signalPathOverlay.toFront (true);
    }
    else if (e.eventComponent == &donateLabel)
    {
        juce::URL ("https://paypal.me/captainkirna").launchInDefaultBrowser();
    }
    else if (e.eventComponent == &logoButton)
    {
        juce::URL ("https://dope.town/").launchInDefaultBrowser();
    }
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Light-blue background.
    g.fillAll (SuomiLookAndFeel::kBackground);
}

void PluginEditor::resized()
{
    // uiRoot is always 1050 × 1007.  Scale it to fit whatever window size the
    // host has given us (set by scaleBox via setSize).  Every child component
    // is a child of uiRoot, so the single transform scales everything uniformly
    // — no individual panel layout code needs to know the current scale.
    const float scale = getHeight() / 1108.0f;
    uiRoot.setTransform (juce::AffineTransform::scale (scale));

    // All layout coordinates below are in uiRoot's fixed 1050 × 980 space.
    auto area = uiRoot.getLocalBounds();   // always 1050 × 980

    static constexpr int kVuMeterW = 54;
    static constexpr int kVuStripW = kVuMeterW * 2;

    // ── Header ────────────────────────────────────────────────────────────────
    auto header = area.removeFromTop (80);
    header.removeFromRight (kVuStripW);
    titleLabel.setBounds  (header.removeFromLeft (450));
    titleLabel.setFont    (juce::Font (93.31f, juce::Font::bold));   // transform handles scale
    // Small "Beta" tag at the lower-right corner of the title, slightly overlapping.
    betaLabel.setBounds (titleLabel.getRight() - 55, titleLabel.getBottom() - 22, 100, 18);
    {
        // Three stacked buttons on the right: Load Sample / Presets / Scale selector.
        auto col = header.removeFromRight (130);
        const int btnH = col.getHeight() / 3;
        loadButton  .setBounds (col.removeFromTop (btnH).reduced (3, 2));
        {
            auto row = col.removeFromTop (btnH).reduced (3, 2);
            prevPresetButton.setBounds (row.removeFromLeft (22));
            nextPresetButton.setBounds (row.removeFromRight (22));
            presetButton    .setBounds (row);
        }
        scaleBox    .setBounds (col.removeFromTop (btnH).reduced (3, 2));

        // ZC toggle and folder button share a 46 px column to the left of the main button column.
        // ZC aligns with Load Sample (row 1); DIR aligns with Presets (row 2); DEF with Scale (row 3).
        auto zcCol = header.removeFromRight (46);
        zcButton        .setBounds (zcCol.removeFromTop (btnH).reduced (3, 2));
        openFolderButton.setBounds (zcCol.removeFromTop (btnH).reduced (3, 2));
        defaultButton   .setBounds (zcCol.removeFromTop (btnH).reduced (3, 2));

        // BQ toggle sits to the left of the ZC column, same row 1 height.
        auto bqCol = header.removeFromRight (46);
        bqButton.setBounds (bqCol.removeFromTop (btnH).reduced (3, 2));
    }
    logoButton.setBounds  (header.reduced (4));
    // "Contact" at the same vertical position and font size as betaLabel, right-aligned next to logo.
    contactLabel.setBounds (logoButton.getRight() - 80, betaLabel.getY(), 75, 18);
    donateLabel.setBounds (contactLabel.getRight() - 10, betaLabel.getY(), 55, 18);

    // ── Waveform strip ────────────────────────────────────────────────────────
    auto waveRow = area.removeFromTop (120);
    waveRow.removeFromRight (kVuStripW);
    waveformDisplay.setBounds (waveRow);

    // ── VU meters with gain knobs overlaid at the bottom ───────────────────
    constexpr int kGainKnobH = 40;
    const int vuH = 80 + 120;
    const int vuX = 1050 - kVuStripW;

    preFxMeter .setBounds (vuX,             0, kVuMeterW, vuH);
    postFxMeter.setBounds (vuX + kVuMeterW, 0, kVuMeterW, vuH);

    // Gain knobs overlaid on the VU meters, lifted slightly from the bottom.
    constexpr int kGainKnobInset = 6;
    preGainSlider .setBounds (vuX,             vuH - kGainKnobH - kGainKnobInset, kVuMeterW, kGainKnobH);
    postGainSlider.setBounds (vuX + kVuMeterW, vuH - kGainKnobH - kGainKnobInset, kVuMeterW, kGainKnobH);

    // ── Sample controls + envelope panels — single combined row ───────────────
    // Row shrunk from 88→76 px to donate space to the granulizer bar.
    {
        auto row = area.removeFromTop (88);
        // EnvelopePanel overhead: reduced(4)=8 vertical, title 14, gap 3, label 13, gap 1.
        const int knobW = juce::jmax (24, juce::jmin (SuomiLookAndFeel::kStdKnobSize,
                                                       row.getHeight() - 39));
        const int volEnvW   = 4 * knobW + 3 * 4 + 8;
        const int pitchEnvW = 5 * knobW + 4 * 4 + 8;

        volEnvPanel  .forceKnobSize (knobW);
        pitchEnvPanel.forceKnobSize (knobW);

        pitchEnvPanel     .setBounds (row.removeFromRight (pitchEnvW));
        volEnvPanel       .setBounds (row.removeFromRight (volEnvW));
        sampleControlPanel.setBounds (row);
    }

    // ── Magixx panel ─────���────────────��───────────────────────────────────────
    // Height shrunk from 168→120 px (knob sizes scale automatically in MagixxPanel::resized).
    magixxPanel.setBounds (area.removeFromTop (168));
    // Granulizer and Stutter share a single 60 px row.
    {
        auto row = area.removeFromTop (60);
        constexpr int granW = 500;
        granulizerPanel.setBounds (row.removeFromLeft (granW));
        stutterPanel.setBounds    (row);
    }
    area.removeFromTop (8);   // gap before Step Controller section

    // ── Granulizer bar ───────────────────────���──────────────────────────────��─

    // ── FX rows ───────────────────────────────────────────────────────────────
    static constexpr int fxRowH = 60;
    {
        auto gateRow  = area.removeFromTop (157);
        auto rightCol = gateRow.removeFromRight (315);
        sidechainPanel     .setBounds (rightCol.removeFromTop (73));
        rightCol.removeFromTop (6);
        arpPanel           .setBounds (rightCol.removeFromTop (73));
        gateControllerPanel.setBounds (gateRow);
    }
    {
        auto filterRow = area.removeFromTop (70);
        paramDisplay.setBounds (filterRow.removeFromRight (270));
        filterPanel  .setBounds (filterRow);
    }
    {
        auto distRow = area.removeFromTop (fxRowH);
        envFollowerPanel.setBounds (distRow.removeFromRight (270));
        distPanel       .setBounds (distRow);
    }
    flangerPanel.setBounds (area.removeFromTop (fxRowH));
    {
        auto phaserRow = area.removeFromTop (fxRowH);
        highcutPanel.setBounds (phaserRow.removeFromRight (270));
        phaserPanel .setBounds (phaserRow);
    }
    {
        auto reverbRow = area.removeFromTop (fxRowH);
        lowcutPanel.setBounds (reverbRow.removeFromRight (270));
        reverbPanel.setBounds (reverbRow);
    }
    {
        auto delayRow = area.removeFromTop (118);
        auto rightCol = delayRow.removeFromRight (270);
        const int halfH = rightCol.getHeight() / 2;
        clipPanel      .setBounds (rightCol.removeFromTop (halfH));
        suppressorPanel.setBounds (rightCol);
        delayPanel     .setBounds (delayRow);
    }

    // Spuge overlay: centred on the spuge slot in the flanger row (local x≈859, uiRoot y≈773).
    // Sized to the spuge slot so resized() computes the correct fit scale; the component draws
    // at 2× that scale, so it bleeds into neighbouring rows underneath them.
    spugeOverlay.setBounds (1050 - 278, 805, 278, 72);

    // Cover art: centred between Amt knob and Clip enable button, bottom-anchored.
    // Underlaps nearby controls — rendered behind all panels (added first to uiRoot).
    coverArt.setBounds (735 - 196 / 2 - 15, 1108 - 354 + 20, 196, 354);
    // Click area: only the visible image portion, narrowed to avoid overlapping knobs.
    coverArtClickArea.setBounds (coverArt.getX() + 55, coverArt.getBottom() - 160,
                                 coverArt.getWidth() - 110, 160);
}

void PluginEditor::parameterChanged (const juce::String& paramId, float)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (
            processor.apvts.getParameter (paramId)))
    {
        const auto name  = p->getName (50);
        const auto value = p->getCurrentValueAsText();
        // Marshal to message thread — parameterChanged can fire on the audio thread.
        juce::MessageManager::callAsync ([this, name, value, paramId]
        {
            paramDisplay.showParameter (name, value, paramId);
        });
    }
}

void PluginEditor::onLoadButtonClicked()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Load Sample", juce::File{}, "*.wav;*.mp3;*.aif;*.aiff");

    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc) {
            const auto results = fc.getResults();
            if (results.isEmpty())
                return;
            const juce::File f = results[0];
            processor.loadSampleFile (f);
            waveformDisplay.setFile (f);
            presetManagerPanel.setCurrentSampleFile (f);
        });
}

//==============================================================================
// Default-reset confirmation overlay
//==============================================================================
juce::Rectangle<int> PluginEditor::DefaultConfirmOverlay::getDialogBounds() const
{
    constexpr int kW = 340;
    constexpr int kH = 110;
    return { (getWidth() - kW) / 2, (getHeight() - kH) / 2, kW, kH };
}

void PluginEditor::DefaultConfirmOverlay::paint (juce::Graphics& g)
{
    // Semi-transparent backdrop.
    g.fillAll (juce::Colours::black.withAlpha (0.45f));

    const auto dlg = getDialogBounds().toFloat();

    // Dialog box background.
    g.setColour (juce::Colour (0xff1a1a2e));
    g.fillRoundedRectangle (dlg, 6.0f);
    g.setColour (juce::Colour (0xff6c63ff));
    g.drawRoundedRectangle (dlg, 6.0f, 1.5f);

    // Prompt text.
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (15.0f));
    g.drawText ("Set default state for all knobs?",
                getDialogBounds().withTrimmedBottom (50),
                juce::Justification::centred, false);
}

void PluginEditor::DefaultConfirmOverlay::resized()
{
    const auto dlg = getDialogBounds();
    const int btnW = 150;
    const int btnH = 28;
    const int btnY = dlg.getBottom() - btnH - 14;
    const int gap  = 12;
    const int totalW = btnW * 2 + gap;
    const int startX = dlg.getX() + (dlg.getWidth() - totalW) / 2;

    yesButton.setBounds (startX, btnY, btnW, btnH);
    noButton .setBounds (startX + btnW + gap, btnY, btnW, btnH);
}

void PluginEditor::DefaultConfirmOverlay::mouseUp (const juce::MouseEvent& e)
{
    // Click outside the dialog dismisses it.
    if (!getDialogBounds().contains (e.getPosition()))
        setVisible (false);
}

void PluginEditor::onDefaultButtonClicked()
{
    if (defaultConfirmOverlay == nullptr)
    {
        defaultConfirmOverlay = std::make_unique<DefaultConfirmOverlay> (*this);
        uiRoot.addChildComponent (*defaultConfirmOverlay);
    }

    defaultConfirmOverlay->setBounds (uiRoot.getLocalBounds());
    defaultConfirmOverlay->setVisible (true);
    defaultConfirmOverlay->toFront (true);
}

void PluginEditor::resetAllParametersToDefault()
{
    auto& apvts = processor.apvts;

    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* paramWithID = dynamic_cast<juce::RangedAudioParameter*> (param))
            paramWithID->setValueNotifyingHost (paramWithID->getDefaultValue());
    }

    waveformDisplay.setPresetName ({});
}
