#include "GateControllerPanel.h"
#include "ParameterDisplay.h"
#include "../ParameterIDs.h"
#include "SuomiLookAndFeel.h"
#include <BinaryData.h>

GateControllerPanel::GateControllerPanel (juce::AudioProcessorValueTreeState& apvts,
                                          GateController& gate)
    : apvtsRef (apvts), gateRef (gate)
{
    // ── Enable toggle ─────────────────────────────────────────────────────────
    addAndMakeVisible (enableButton);
    enableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParamIDs::gateEnabled, enableButton);

    // ── Step count slider ─────────────────────────────────────────────────────
    stepCountLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (stepCountLabel);

    stepCountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    stepCountSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 28, 16);
    stepCountSlider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (stepCountSlider);
    stepCountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParamIDs::gateStepCount, stepCountSlider);

    // ── Gate volume fader ──────────────────────────────────────────────────────
    gateFader = std::make_unique<DuckFaderButton> (apvts, ParamIDs::gateVolumeAmt, "Gate");
    gateFader->setTooltip (
        "Trance Gate\n\n"
        "Controls how much the step sequencer modulates the output volume. "
        "At 0% the steps have no effect on volume. At 100% the step levels "
        "fully gate the signal - steps set to zero will silence the audio.");
    addAndMakeVisible (*gateFader);

    // ── Routing button ─────────────────────────────────────────────────────────
    routeButton.setTooltip (
        "TRANCE GATE routing\n\n"
        "A = Pre FX (before effects chain)\n"
        "B = Pre Reverb (after Phaser)\n"
        "C = Post FX (after Delay)");
    routeButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    addAndMakeVisible (routeButton);
    {
        auto* param = apvtsRef.getParameter (ParamIDs::gateRouting);
        auto updateRouteLabel = [this, param]
        {
            const int idx = static_cast<int> (param->convertFrom0to1 (param->getValue()));
            static const char* labels[] = { "A", "B", "C" };
            routeButton.setButtonText (labels[juce::jlimit (0, 2, idx)]);
            routeButton.repaint();
        };
        routeButton.onClick = [param, updateRouteLabel]
        {
            const int cur = static_cast<int> (param->convertFrom0to1 (param->getValue()));
            const int next = (cur + 1) % 3;
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (next)));
            updateRouteLabel();
        };
        updateRouteLabel();
    }

    // ── Division combo ────────────────────────────────────────────────────────
    // 1/64 displayed first; item IDs must match the parameter choice indices (1-based).
    divBox.addItem ("1/64", 12);
    const juce::StringArray divNames {
        "1/32","1/16","1/8","1/8D","1/4","1/4D","1/2","1/2D","1/1","1/1D","2/1"
    };
    for (int i = 0; i < divNames.size(); ++i)
        divBox.addItem (divNames[i], i + 1);
    addAndMakeVisible (divBox);
    divAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParamIDs::gateDivision, divBox);

    // ── Pattern buttons ───────────────────────────────────────────────────────
    // showActionMessage is posted via callAsync so it runs AFTER the APVTS
    // AsyncUpdater fires its parameterChanged notifications.  Without the
    // defer, the editor's listener would overwrite our message with the last
    // step's numeric value on every first press (and always for Bin).
    maxButton    .onClick = [this] { setAllSteps (1.0f);     juce::MessageManager::callAsync ([this] { showActionMessage ("All set to maximum"); }); };
    minButton    .onClick = [this] { setAllSteps (0.0f);     juce::MessageManager::callAsync ([this] { showActionMessage ("All set to minimum"); }); };
    randButton   .onClick = [this] { randomizeSteps();       juce::MessageManager::callAsync ([this] { showActionMessage ("Randomised"); }); };
    randBinButton.onClick = [this] { randomizeStepsBinary(); juce::MessageManager::callAsync ([this] { showActionMessage ("Rnd to max and min"); }); };
    addAndMakeVisible (maxButton);
    addAndMakeVisible (minButton);
    addAndMakeVisible (randButton);
    addAndMakeVisible (randBinButton);

    // ── Step sliders ──────────────────────────────────────────────────────────
    for (int i = 0; i < GateController::kMaxSteps; ++i)
    {
        stepSliders[i].setSliderStyle (juce::Slider::LinearVertical);
        stepSliders[i].setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        // Steps 4, 12, 20, 28 (indices 3, 11, 19, 27) get a darker track channel.
        if (i % 8 == 4)
            stepSliders[i].getProperties().set ("darkTrack", true);
        addAndMakeVisible (stepSliders[i]);
        stepAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, ParamIDs::gateStepId (i).c_str(), stepSliders[i]);
    }

    // Dim all controls (except the enable button itself) when the step controller
    // is disabled. Called on every click and once at construction for restored state.
    auto applyDim = [this]
    {
        const float a = enableButton.getToggleState() ? 1.0f : 0.35f;
        stepCountLabel .setAlpha (a);
        stepCountSlider.setAlpha (a);
        gateFader      ->setAlpha (a);
        routeButton    .setAlpha (a);
        divBox         .setAlpha (a);
        maxButton      .setAlpha (a);
        minButton      .setAlpha (a);
        randButton     .setAlpha (a);
        randBinButton  .setAlpha (a);
        for (auto& sl : stepSliders)
            sl.setAlpha (a);
        repaint();
    };
    enableButton.onClick = applyDim;
    applyDim();

    decorImage = juce::ImageCache::getFromMemory (BinaryData::_02780_png, BinaryData::_02780_pngSize);


    startTimerHz (30);
}

GateControllerPanel::~GateControllerPanel()
{
    stopTimer();
}

void GateControllerPanel::setParameterDisplay (ParameterDisplay* display)
{
    paramDisplay = display;
}

void GateControllerPanel::showActionMessage (const juce::String& message)
{
    if (paramDisplay != nullptr)
        paramDisplay->showParameter ("Step Controller", message, {});
}

void GateControllerPanel::setAllSteps (float normalizedValue)
{
    for (int i = 0; i < GateController::kMaxSteps; ++i)
        if (auto* p = apvtsRef.getParameter (ParamIDs::gateStepId (i).c_str()))
            p->setValueNotifyingHost (normalizedValue);
}

void GateControllerPanel::randomizeSteps()
{
    for (int i = 0; i < GateController::kMaxSteps; ++i)
        if (auto* p = apvtsRef.getParameter (ParamIDs::gateStepId (i).c_str()))
            p->setValueNotifyingHost (rng.nextFloat());
}

void GateControllerPanel::randomizeStepsBinary()
{
    for (int i = 0; i < GateController::kMaxSteps; ++i)
        if (auto* p = apvtsRef.getParameter (ParamIDs::gateStepId (i).c_str()))
            p->setValueNotifyingHost (rng.nextBool() ? 1.0f : 0.0f);
}

void GateControllerPanel::timerCallback()
{
    const int step = gateRef.currentStepForUI.load (std::memory_order_relaxed);
    if (step != lastDisplayedStep)
    {
        lastDisplayedStep = step;
        repaint();
    }

    // Gray out routing button when trance gate amount is zero.
    const float gateAmt = apvtsRef.getRawParameterValue (ParamIDs::gateVolumeAmt)->load();
    const bool routeActive = enableButton.getToggleState() && gateAmt > 0.0f;
    const float routeAlpha = routeActive ? 1.0f : 0.35f;
    if (routeButton.getAlpha() != routeAlpha)
        routeButton.setAlpha (routeAlpha);
}

void GateControllerPanel::paint (juce::Graphics& g)
{
    // Panel background — same blue hue as the main UI background.
    // Round left corners only — right edge is flush against the sidechain/arp panels.
    g.setColour (SuomiLookAndFeel::kBackground.withAlpha (0.30f));
    {
        juce::Path p;
        auto b = getLocalBounds().toFloat();
        p.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(),
                               6.0f, 6.0f, true, false, true, false);
        g.fillPath (p);
    }

    // Highlight the active step and dim inactive steps.
    // Use actual slider bounds so the overlays are pixel-perfect regardless of padding.
    const int activeStep = lastDisplayedStep;
    const int stepCount  = static_cast<int> (stepCountSlider.getValue());

    // Overlay height: slider area + step-number strip, leaving kBottomGap clear at the bottom.
    const float overlayH = static_cast<float> (stepSliders[0].getBounds().getBottom() + 20);

    if (activeStep >= 0 && activeStep < stepCount)
    {
        const auto sb = stepSliders[activeStep].getBounds();
        g.setColour (SuomiLookAndFeel::kPurpleLight.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> ((float) sb.getX(), 0.0f, (float) sb.getWidth(), overlayH));
    }

    // Dim inactive steps (beyond pattern length).
    g.setColour (juce::Colour (0x55000000));
    for (int i = stepCount; i < GateController::kMaxSteps; ++i)
    {
        const auto sb = stepSliders[i].getBounds();
        g.fillRect (juce::Rectangle<float> ((float) sb.getX(), 0.0f, (float) sb.getWidth(), overlayH));
    }

    // Decorative image below Rnd/Bin buttons, scaled to fit while preserving aspect ratio.
    if (decorImage.isValid() && !decorImageArea.isEmpty())
    {
        g.setOpacity (0.55f);
        g.drawImage (decorImage, decorImageArea.toFloat(),
                     juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);
    }

    // Step numbers below each slider — drawn last so they appear above overlays.
    constexpr int stepNumH = 20;
    if (auto* laf = dynamic_cast<SuomiLookAndFeel*> (&getLookAndFeel()))
        g.setFont (laf->getLabelFontBold (18.46f));
    else
        g.setFont (juce::Font (18.46f, juce::Font::bold));
    const float textAlpha = enableButton.getToggleState() ? 1.0f : 0.35f;
    g.setColour (juce::Colours::white.withAlpha (textAlpha));
    for (int i = 0; i < GateController::kMaxSteps; ++i)
    {
        const auto sb = stepSliders[i].getBounds();
        g.drawFittedText (juce::String (i + 1),
                          sb.getX(), sb.getBottom() + 1, sb.getWidth(), stepNumH,
                          juce::Justification::centred, 1);
    }
}

void GateControllerPanel::resized()
{
    auto area = getLocalBounds().reduced (6);

    constexpr int leftW  = 120;   // left controls strip
    constexpr int gap    = 4;
    constexpr int rowH   = 22;
    constexpr int lblH   = 14;

    auto left = area.removeFromLeft (leftW);
    area.removeFromLeft (gap);

    // Enable toggle: placed at Y=0 (above the reduced area) so its top aligns
    // with the SidechainPanel modeBox, which also starts at Y=0.
    enableButton.setBounds (left.getX(), 0, left.getWidth(), rowH);
    left.removeFromTop (rowH);   // advance cursor as normal so rows below are unaffected
    left.removeFromTop (gap);

    // Steps row: label + slider.
    {
        auto row = left.removeFromTop (rowH);
        stepCountLabel.setBounds (row.removeFromLeft (36).withHeight (lblH));
        stepCountSlider.setBounds (row);
    }
    left.removeFromTop (gap);

    // Gate + Route + Div row: gate fader + routing button + division combo.
    {
        auto row = left.removeFromTop (rowH);
        constexpr int routeW = 18;
        const int divW = row.getWidth() / 2;            // 60px — unchanged
        const int faderW = row.getWidth() - divW - routeW; // 42px
        gateFader->setBounds   (row.removeFromLeft (faderW));
        routeButton.setBounds  (row.removeFromLeft (routeW));
        divBox.setBounds       (row);
    }
    left.removeFromTop (gap);

    // Pattern buttons — two rows of two.
    {
        auto row = left.removeFromTop (rowH);
        const int btnW = row.getWidth() / 2;
        maxButton.setBounds (row.removeFromLeft (btnW));
        minButton.setBounds (row);
    }
    left.removeFromTop (gap);
    {
        auto row = left.removeFromTop (rowH);
        const int btnW = row.getWidth() / 2;
        randButton   .setBounds (row.removeFromLeft (btnW));
        randBinButton.setBounds (row);
    }

    // Remaining left area below Rnd/Bin: used for the decorative image.
    left.removeFromTop (gap);
    decorImageArea = left.expanded (2);

    // Reserve bottom strip for step-number labels plus a gap that separates the
    // step area from the FX section below — sliders are shortened accordingly.
    constexpr int stepNumH    = 20;
    constexpr int kBottomGap  = 5;
    area.removeFromBottom (stepNumH + kBottomGap);
    const int n    = GateController::kMaxSteps;
    const int x0   = area.getX();
    const int totalW = area.getWidth();
    for (int i = 0; i < n; ++i)
    {
        const int x    = x0 + juce::roundToInt ((float) i       * totalW / n);
        const int xEnd = x0 + juce::roundToInt ((float) (i + 1) * totalW / n);
        stepSliders[i].setBounds (x, area.getY(), xEnd - x, area.getHeight());
    }
}
