#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "WaveformMarker.h"

#include <juce_core/juce_core.h>
#include <memory>
#include <functional>

class SamplerEngine;        // forward declaration — full type included in .cpp
class GranulizerProcessor;  // forward declaration

class WaveformDisplay : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        public juce::Timer,
                        private juce::ChangeListener
{
public:
    WaveformDisplay (juce::AudioProcessorValueTreeState& apvts,
                     juce::AudioFormatManager& formatManager,
                     juce::AudioThumbnailCache& cache);
    ~WaveformDisplay() override;

    void paint   (juce::Graphics& g) override;
    void resized () override;

    // Load a new file into the thumbnail. Also clears any missing-sample warning.
    void setFile (const juce::File& file);

    // Set the active preset name so the badge can show it separately from the sample filename.
    // Pass an empty string to clear (e.g. when loading a raw sample without a preset).
    void setPresetName (const juce::String& name);

    // Show a "sample missing" warning overlay. Cleared automatically by setFile().
    void setMissingSample (const juce::String& filename);

    // Called when a new sample is loaded; repositions markers to current param values.
    void syncMarkersToParameters();

    // Provide the engine so the display can read the playback position.
    void setEngine (SamplerEngine* engine);

    // Provide the granulizer so the display can show the grain read position.
    void setGranulizer (GranulizerProcessor* granulizer);

    // Provide the normalize gain source so the waveform scales visually when NORM is on.
    void setNormalizeGainSource (std::atomic<float>* src) { normalizeGainSrc = src; }


    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;

    // Callback invoked when the user drops a valid audio file.
    std::function<void (const juce::File&)> onFileDrop;

    // Fired when auto-tune changes the root note (so the UI can highlight the selector).
    std::function<void()> onRootNoteChanged;

private:
    // Custom LookAndFeel for the zoom slider: draws the thumb as a rotating spiral.
    struct ZoomSliderLAF : public juce::LookAndFeel_V4
    {
        void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;

        // Extra rotation accumulated from drag velocity; decayed by the 30 Hz timer.
        float velocityRotation { 0.0f };
    };

    // Transparent overlay drawn on top of the markers; paints the filename badge
    // and the normalize toggle.  Added last so it renders above the draggable markers.
    struct BadgeOverlay : public juce::Component,
                          public juce::TooltipClient
    {
        explicit BadgeOverlay (WaveformDisplay& o) : owner (o)
        {
            setInterceptsMouseClicks (true, false);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        bool hitTest (int x, int y) override;
        juce::String getTooltip() override;
        juce::Rectangle<int> getBadgeBounds() const;
        juce::Rectangle<int> getNormBadgeBounds() const;
        juce::Rectangle<int> getTuneBadgeBounds() const;

        bool         tuneInProgress     { false };
        juce::String tuneResultText;
        int          tuneResultCountdown { 0 };   // timer ticks remaining for result flash

        WaveformDisplay& owner;
    };

    // ChangeListener — called by AudioThumbnail when waveform data is ready.
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    // Timer — polls playback position at 30 Hz and repaints when it changes.
    void timerCallback() override;

    void onStartMarkerMoved (float normPos);
    void onEndMarkerMoved   (float normPos);

    // Recompute viewStart/viewWidth from current zoom and selection midpoint,
    // then reposition markers to match the new view.
    void updateViewWindow();

    // Converts a normalised (0–1) position to a pixel X within the waveform area.
    float normToPixelX (float norm, float areaWidth) const;

    // Height reserved at the bottom of the component for the zoom slider.
    static constexpr int kZoomSliderH   { 13 };
    // Width reserved on the left of the zoom strip for the "Zoom" label.
    static constexpr int kZoomLabelW    { 42 };

    juce::AudioProcessorValueTreeState& apvts;
    juce::AudioThumbnail                thumbnail;

    // Zoom slider, its custom LAF, and its label.
    ZoomSliderLAF zoomSliderLAF;
    juce::Label   zoomLabel  { {}, "Zoom" };
    juce::Slider  zoomSlider;

    // Current view window in normalised (0–1) space.
    float viewStart { 0.0f };
    float viewWidth { 1.0f };

    WaveformMarker startMarker { WaveformMarker::Type::Start };
    WaveformMarker endMarker   { WaveformMarker::Type::End };

    static constexpr float kMarkerEpsilon { 0.005f };

    juce::String   loadedFileName;               // displayed in the lower-right corner
    juce::String   presetName;                   // set when loaded via preset manager
    juce::File     loadedFile;                   // full path — used to open parent directory on click

    // Returns the sample filename with the preset prefix stripped (if any).
    juce::String getDisplaySampleName() const;
    bool           missingFile      { false };
    juce::String   missingFileName;
    juce::String   searchStatus;    // shown after a folder search ("not found" or empty)

    juce::TextButton searchFolderButton { "Search in folder..." };
    // Kept alive across the async callback.
    std::shared_ptr<juce::FileChooser> folderChooser;

    void onSearchFolderClicked();

    // Rendered on top of the markers (added last in the constructor).
    BadgeOverlay badgeOverlay { *this };

    SamplerEngine*        engine            { nullptr };
    GranulizerProcessor*  granulizer        { nullptr };
    std::atomic<float>*   normalizeGainSrc  { nullptr };  // points to PluginProcessor::normalizeGain
    float                 cachedPlaybackPos { -1.0f };  // message-thread copy, updated by timer
    float                 cachedGranPos     { -1.0f };  // granulizer grain read position (normalised)
    float                 cachedStartPos    { -1.0f };  // polled by timer to detect automation changes
    float                 cachedEndPos      { -1.0f };


    // Tracks the previous normalised zoom value so onValueChange can compute velocity.
    float zoomPrevNorm { 0.0f };

    // Slicer state — polled by timer so markers update when key or count changes.
    bool cachedSlicerEnabled { false };
    int  cachedSlicerLoNote  { 60 };
    int  cachedSliceCount    { 16 };

    // Cached normalize state — repaint waveform when toggled.
    bool cachedNormEnabled { false };

    // ── Pitch detection (auto-tune) ──────────────────────────────────────────
    class PitchDetectionJob;
    class TuneResultOverlay;
    std::unique_ptr<PitchDetectionJob> pitchJob;
    std::unique_ptr<TuneResultOverlay> tuneOverlay;
    int previousRootNote { 48 };   // saved before tuning so "No way!" can revert
    void launchPitchDetection();
    void onPitchDetected (int midiNote, float freqHz, float confidence);
    void onPitchDetectionFailed();
    void showTuneOverlay (const juce::String& noteName, bool success, int newNote,
                          float confidence);
    void dismissTuneOverlay (bool revert);
};
