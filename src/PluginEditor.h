#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <BinaryData.h>

#include "PluginProcessor.h"
#include "ui/SuomiLookAndFeel.h"
#include "ui/WaveformDisplay.h"
#include "ui/SampleControlPanel.h"
#include "ui/MagixxPanel.h"
#include "ui/EnvelopePanel.h"
#include "ui/FilterPanel.h"
#include "ui/DistortionPanel.h"
#include "ui/FlangerPanel.h"
#include "ui/PhaserPanel.h"
#include "ui/ReverbPanel.h"
#include "ui/DelayPanel.h"
#include "ui/GateControllerPanel.h"
#include "ui/SidechainPanel.h"
#include "ui/EnvFollowerPanel.h"
#include "ui/ArpeggiatorPanel.h"
#include "ui/ClipPanel.h"
#include "ui/HighcutPanel.h"
#include "ui/LowcutPanel.h"
#include "ui/ParameterDisplay.h"
#include "ui/VuMeter.h"
#include "ui/PresetManagerPanel.h"
#include "ui/GranulizerPanel.h"
#include "ui/StutterPanel.h"
#include "ui/SuppressorPanel.h"

// Non-interactive image overlay — scales to fit its bounds, passes all mouse events through.
struct CoverArtComponent : public juce::Component
{
    CoverArtComponent()
    {
        sourceImage = juce::ImageCache::getFromMemory (
            BinaryData::Kirna__Glitter2000_cover_for_vst_png,
            BinaryData::Kirna__Glitter2000_cover_for_vst_pngSize);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void resized() override
    {
        if (! sourceImage.isValid() || getWidth() <= 0 || getHeight() <= 0)
            return;

        const float scale = juce::jmin ((float) getWidth()  / (float) sourceImage.getWidth(),
                                        (float) getHeight() / (float) sourceImage.getHeight());
        scaledImage = sourceImage.rescaled (juce::roundToInt (sourceImage.getWidth()  * scale),
                                            juce::roundToInt (sourceImage.getHeight() * scale),
                                            juce::Graphics::highResamplingQuality);
    }

    void paint (juce::Graphics& g) override
    {
        if (! scaledImage.isValid())
            return;

        const int x = (getWidth()  - scaledImage.getWidth())  / 2;
        const int y =  getHeight() - scaledImage.getHeight();   // bottom-aligned
        g.drawImageAt (scaledImage, x, y);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        juce::URL ("https://dope.town/").launchInDefaultBrowser();
    }

private:
    juce::Image sourceImage;
    juce::Image scaledImage;
};

// Spuge2 image overlay — renders at 2x fit size, passes mouse events through,
// and layers beneath all sibling panels when added early to uiRoot.
struct SpugeOverlayComponent : public juce::Component
{
    SpugeOverlayComponent()
    {
        sourceImage = juce::ImageCache::getFromMemory (
            BinaryData::spuge2_png, BinaryData::spuge2_pngSize);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void resized() override
    {
        if (! sourceImage.isValid() || getWidth() <= 0 || getHeight() <= 0)
            return;

        // Reference slot size matches spugeBounds in FlangerPanel (361 × 48).
        // Using fixed reference keeps the rendered size independent of this component's bounds.
        const float fitScale = juce::jmin (361.0f / (float) sourceImage.getWidth(),
                                           48.0f  / (float) sourceImage.getHeight());
        const float scale = fitScale * 1.5f;
        scaledImage = sourceImage.rescaled (juce::roundToInt (sourceImage.getWidth()  * scale),
                                            juce::roundToInt (sourceImage.getHeight() * scale),
                                            juce::Graphics::highResamplingQuality);
    }

    void paint (juce::Graphics& g) override
    {
        if (! scaledImage.isValid())
            return;

        // Paint line art in the same grey that om/alleye appear as on the panel.
        // fillAlphaChannelWithCurrentColour uses the image alpha as a stencil.
        g.setColour (juce::Colour (0xff9495ad));
        const int x = (getWidth()  - scaledImage.getWidth())  / 2;
        const int y = (getHeight() - scaledImage.getHeight()) / 2;
        g.drawImageAt (scaledImage, x, y, true);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        juce::URL ("https://www.youtube.com/watch?v=JMJkHQIgqCY&list=PLVbC7883e3Zw").launchInDefaultBrowser();
    }

private:
    juce::Image sourceImage;
    juce::Image scaledImage;
};

// Displays DOPE.png scaled to fit and opens the Bandcamp page on click.
struct LogoButton : public juce::Component, public juce::SettableTooltipClient
{
    LogoButton()
    {
        sourceImage = juce::ImageCache::getFromMemory (BinaryData::DOPEsmall_png, BinaryData::DOPEsmall_pngSize);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void resized() override
    {
        if (! sourceImage.isValid() || getWidth() <= 0 || getHeight() <= 0)
            return;

        // Pre-scale once at resize time so paint is a crisp 1:1 blit.
        const float scale = juce::jmin ((float) getWidth()  / (float) sourceImage.getWidth(),
                                        (float) getHeight() / (float) sourceImage.getHeight());
        scaledImage = sourceImage.rescaled (juce::roundToInt (sourceImage.getWidth()  * scale),
                                            juce::roundToInt (sourceImage.getHeight() * scale),
                                            juce::Graphics::highResamplingQuality);
    }

    void paint (juce::Graphics& g) override
    {
        if (! scaledImage.isValid())
            return;

        const int x = (getWidth()  - scaledImage.getWidth())  / 2;
        const int y = (getHeight() - scaledImage.getHeight()) / 2;
        g.drawImageAt (scaledImage, x, y);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        juce::URL ("https://dopescience.bandcamp.com/").launchInDefaultBrowser();
    }

private:
    juce::Image sourceImage;
    juce::Image scaledImage;
};

// Full-screen overlay showing the signal path flowchart, dismissed with an X button.
struct SignalPathOverlay : public juce::Component
{
    SignalPathOverlay()
    {
        sourceImage = juce::ImageCache::getFromMemory (
            BinaryData::signal_path_simple_png, BinaryData::signal_path_simple_pngSize);

        closeButton.setButtonText ("X");
        closeButton.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeButton);

        setVisible (false);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.92f));

        if (! scaledImage.isValid())
            return;

        const int x = (getWidth()  - scaledImage.getWidth())  / 2;
        const int y = (getHeight() - scaledImage.getHeight()) / 2;
        g.drawImageAt (scaledImage, x, y);
    }

    void resized() override
    {
        constexpr int btnSize = 30;
        closeButton.setBounds (getWidth() - btnSize - 8, 8, btnSize, btnSize);

        if (sourceImage.isValid() && getWidth() > 0 && getHeight() > 0)
        {
            const float scale = juce::jmin ((float) (getWidth()  - 20) / (float) sourceImage.getWidth(),
                                            (float) (getHeight() - 20) / (float) sourceImage.getHeight());
            scaledImage = sourceImage.rescaled (juce::roundToInt (sourceImage.getWidth()  * scale),
                                                juce::roundToInt (sourceImage.getHeight() * scale),
                                                juce::Graphics::highResamplingQuality);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        setVisible (false);
    }

private:
    juce::Image      sourceImage;
    juce::Image      scaledImage;
    juce::TextButton closeButton;
};

class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor& p);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    PluginProcessor& processor;

    SuomiLookAndFeel suomiLAF;  // must outlive all child components

    // Fixed-size container (1050 × 1007) for all UI children.
    // A scale transform on this component handles 75 % / 50 % scaling
    // without touching any child layout code.
    juce::Component uiRoot;

    // Header
    juce::Label        titleLabel;
    juce::Label        betaLabel;
    LogoButton         logoButton;
    juce::Label        kvrLabel;
    juce::Label        contactLabel;
    juce::Label        donateLabel;
    juce::ToggleButton bqButton          { "BQ" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bqAttach;
    juce::ToggleButton zcButton          { "ZC" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> zcAttach;
    juce::TextButton   loadButton        { "Load Sample" };
    juce::TextButton   presetButton      { "Presets" };
    juce::TextButton   prevPresetButton  { "<" };
    juce::TextButton   nextPresetButton  { ">" };
    juce::TextButton   openFolderButton  { "DIR" };
    juce::TextButton   defaultButton     { "INIT" };
    ParameterDisplay   paramDisplay;
    juce::ComboBox     scaleBox;
    juce::TooltipWindow tooltipWindow { nullptr, 700 };

    PresetManagerPanel presetManagerPanel;

    // Waveform
    WaveformDisplay waveformDisplay;

    // VU meters — live peak display before and after the FX chain.
    VuMeterComponent preFxMeter  { "PRE" };
    VuMeterComponent postFxMeter { "POST" };

    // Pre/Post gain knobs — positioned above their respective VU meters.
    juce::Slider preGainSlider;
    juce::Label  preGainLabel { {}, "Pre" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preGainAttach;

    juce::Slider postGainSlider;
    juce::Label  postGainLabel { {}, "Post" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> postGainAttach;

    // Parameter panels
    SampleControlPanel sampleControlPanel;
    EnvelopePanel      volEnvPanel;
    EnvelopePanel      pitchEnvPanel;
    MagixxPanel        magixxPanel;

    // FX panels
    GranulizerPanel     granulizerPanel;
    StutterPanel        stutterPanel;
    GateControllerPanel gateControllerPanel;
    SidechainPanel      sidechainPanel;
    ArpeggiatorPanel    arpPanel;
    FilterPanel         filterPanel;
    EnvFollowerPanel    envFollowerPanel;
    DistortionPanel  distPanel;
    FlangerPanel    flangerPanel;
    ReverbPanel     reverbPanel;
    PhaserPanel     phaserPanel;
    HighcutPanel    highcutPanel;
    DelayPanel      delayPanel;
    LowcutPanel     lowcutPanel;
    ClipPanel       clipPanel;

    SuppressorPanel suppressorPanel;

    // Decorative overlay: spans phaserRow + delayRow, X between Amt knob and Clip enable button.
    CoverArtComponent coverArt;
    juce::TextButton  coverArtClickArea;

    // Spuge image overlay: centred on the spuge slot in the flanger row, renders at 2x fit size
    // and bleeds into neighbouring rows. Added early to uiRoot so it paints beneath all panels.
    SpugeOverlayComponent spugeOverlay;

    // APVTS::Listener — notified on parameter changes to update paramDisplay.
    void parameterChanged (const juce::String& paramId, float newValue) override;

    // One-shot timer: re-reads the preset name and sample from the processor
    // shortly after construction, catching DAWs that call setStateInformation
    // after createEditor.
    void timerCallback() override;

    void onLoadButtonClicked();
    void onDefaultButtonClicked();
    void resetAllParametersToDefault();

    // Modal overlay for the "reset to defaults" confirmation prompt.
    struct DefaultConfirmOverlay : public juce::Component
    {
        explicit DefaultConfirmOverlay (PluginEditor& ed) : editor (ed)
        {
            yesButton.onClick = [this] { editor.resetAllParametersToDefault(); setVisible (false); };
            noButton .onClick = [this] { setVisible (false); };
            addAndMakeVisible (yesButton);
            addAndMakeVisible (noButton);
        }

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent& e) override;

    private:
        PluginEditor&    editor;
        juce::TextButton yesButton { "Khyl" };
        juce::TextButton noButton  { "Ei mun takii tarvi" };

        juce::Rectangle<int> getDialogBounds() const;
    };
    std::unique_ptr<DefaultConfirmOverlay> defaultConfirmOverlay;

    SignalPathOverlay signalPathOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
