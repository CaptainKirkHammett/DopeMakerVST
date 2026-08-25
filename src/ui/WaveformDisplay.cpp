#include "WaveformDisplay.h"
#include "../ParameterIDs.h"
#include "../engine/SamplerEngine.h"
#include "../dsp/GranulizerProcessor.h"

// Draws the filename badge and normalize toggle.  Lives in a child component
// added after the markers so it paints on top of them.
void WaveformDisplay::BadgeOverlay::paint (juce::Graphics& g)
{
    constexpr int   kPadX   = 6;
    constexpr float kFontSz = 11.0f;

    const juce::Font fontRegular (juce::Font::getDefaultMonospacedFontName(), kFontSz, juce::Font::plain);
    const juce::Font fontBold    (juce::Font::getDefaultMonospacedFontName(), kFontSz, juce::Font::bold);

    // --- Filename badge (lower-right) ---
    if (owner.loadedFileName.isNotEmpty())
    {
        const auto badge = getBadgeBounds();
        g.setColour (juce::Colour (0xcc0d1a2e));
        g.fillRoundedRectangle (badge.toFloat(), 3.0f);

        const auto sampleName = owner.getDisplaySampleName();
        const auto inner = badge.reduced (kPadX, 0);

        if (owner.presetName.isNotEmpty())
        {
            // Two-part display: "PresetName sampleName" — regular + bold.
            const juce::String presetPart = owner.presetName + " ";
            const int presetW = fontRegular.getStringWidth (presetPart);

            g.setColour (juce::Colours::white.withAlpha (0.65f));
            g.setFont (fontRegular);
            g.drawText (presetPart, inner.getX(), inner.getY(), presetW, inner.getHeight(),
                        juce::Justification::centredLeft, false);

            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.setFont (fontBold);
            g.drawText (sampleName, inner.getX() + presetW, inner.getY(),
                        inner.getWidth() - presetW, inner.getHeight(),
                        juce::Justification::centredLeft, true);
        }
        else
        {
            // No preset — show just the sample name in bold.
            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.setFont (fontBold);
            g.drawText (sampleName, inner, juce::Justification::centred, true);
        }
    }

    // --- Normalize toggle badge (above filename badge, right-aligned) ---
    if (owner.loadedFileName.isNotEmpty())
    {
        const bool normOn = owner.apvts.getRawParameterValue (ParamIDs::normalizeEnabled)->load() > 0.5f;
        const auto normBadge = getNormBadgeBounds();
        const auto bgColour = normOn ? juce::Colour (0xcc003580)    // active: Finnish blue
                                     : juce::Colour (0xcc0d1a2e);  // inactive: dark navy
        g.setColour (bgColour);
        g.fillRoundedRectangle (normBadge.toFloat(), 3.0f);

        g.setColour (normOn ? juce::Colours::white
                            : juce::Colours::white.withAlpha (0.55f));
        g.drawText ("NORM", normBadge.reduced (kPadX, 0), juce::Justification::centred, true);
    }
}

juce::Rectangle<int> WaveformDisplay::BadgeOverlay::getBadgeBounds() const
{
    if (owner.loadedFileName.isEmpty())
        return {};

    constexpr int   kPadX   = 6;
    constexpr int   kPadY   = 3;
    constexpr int   kMargin = 4;
    constexpr float kFontSz = 11.0f;

    const juce::Font fontRegular (juce::Font::getDefaultMonospacedFontName(), kFontSz, juce::Font::plain);
    const juce::Font fontBold    (juce::Font::getDefaultMonospacedFontName(), kFontSz, juce::Font::bold);

    int textW;
    if (owner.presetName.isNotEmpty())
    {
        const juce::String presetPart = owner.presetName + " ";
        textW = fontRegular.getStringWidth (presetPart)
              + fontBold.getStringWidth (owner.getDisplaySampleName());
    }
    else
    {
        textW = fontBold.getStringWidth (owner.loadedFileName);
    }
    const int textH = static_cast<int> (fontRegular.getHeight());

    const int bgW = textW + kPadX * 2;
    const int bgH = textH + kPadY * 2;
    const int bgX = getWidth() - bgW - kMargin;
    const int waveH = getHeight() - WaveformDisplay::kZoomSliderH;
    const int bgY = waveH - bgH - kMargin;

    return { bgX, bgY, bgW, bgH };
}

juce::Rectangle<int> WaveformDisplay::BadgeOverlay::getNormBadgeBounds() const
{
    if (owner.loadedFileName.isEmpty())
        return {};

    // Position immediately to the left of the filename badge, same row.
    const auto fileBadge = getBadgeBounds();
    if (fileBadge.isEmpty())
        return {};

    constexpr int   kPadX   = 6;
    constexpr int   kGap    = 3;   // horizontal gap between NORM and filename badge
    constexpr float kFontSz = 11.0f;

    juce::Font f (juce::Font::getDefaultMonospacedFontName(), kFontSz, juce::Font::plain);
    const int textW = f.getStringWidth ("NORM");

    const int bgW = textW + kPadX * 2;
    const int bgH = fileBadge.getHeight();
    const int bgX = fileBadge.getX() - bgW - kGap;
    const int bgY = fileBadge.getY();

    return { bgX, bgY, bgW, bgH };
}

bool WaveformDisplay::BadgeOverlay::hitTest (int x, int y)
{
    return getBadgeBounds().contains (x, y)
        || getNormBadgeBounds().contains (x, y);
}

void WaveformDisplay::BadgeOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (getNormBadgeBounds().contains (e.getPosition()))
    {
        if (auto* p = owner.apvts.getParameter (ParamIDs::normalizeEnabled))
        {
            const bool nowOn = p->getValue() > 0.5f;
            p->setValueNotifyingHost (nowOn ? 0.0f : 1.0f);
        }
        repaint();
        return;
    }

    if (owner.loadedFile.existsAsFile())
        owner.loadedFile.revealToUser();
}

// Draws the zoom slider with a spiral thumb that rotates as the value changes.
void WaveformDisplay::ZoomSliderLAF::drawLinearSlider (juce::Graphics& g,
                                                        int x, int y, int width, int height,
                                                        float sliderPos, float /*minPos*/, float /*maxPos*/,
                                                        juce::Slider::SliderStyle /*style*/,
                                                        juce::Slider& slider)
{
    const float cy        = y + height * 0.5f;
    const float trackW    = juce::jmin (4.0f, height * 0.30f);

    // Background track.
    {
        juce::Path bg;
        bg.startNewSubPath ((float) x, cy);
        bg.lineTo           ((float) (x + width), cy);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.strokePath (bg, juce::PathStrokeType (trackW, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // Active fill — left edge to thumb.
    {
        juce::Path fill;
        fill.startNewSubPath ((float) x, cy);
        fill.lineTo           (sliderPos, cy);
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.strokePath (fill, juce::PathStrokeType (trackW, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Spiral thumb — rotates proportionally to the slider value.
    {
        const float strokeW   = 1.2f;
        // Spiral is intentionally larger than the slider strip; it overflows into
        // the waveform area above (enabled by setPaintingIsUnclipped on the slider).
        const float outerR    = 11.0f;  // fixed size — independent of strip height
        const float innerR    = 0.5f;
        const float numTurns  = 3.5f;

        // Map value to rotation: 3 full turns across the whole range,
        // plus any velocity-based extra spin accumulated during dragging.
        const float norm     = static_cast<float> (
            (slider.getValue() - slider.getMinimum()) /
            (slider.getMaximum() - slider.getMinimum()));
        const float rotation = norm * 3.0f * juce::MathConstants<float>::twoPi
                               + velocityRotation;

        const float totalAngle = numTurns * juce::MathConstants<float>::twoPi;
        constexpr int kSteps   = 120;

        juce::Path spiral;
        for (int i = 0; i <= kSteps; ++i)
        {
            const float t  = static_cast<float> (i) / kSteps * totalAngle;
            const float r  = innerR + (outerR - innerR) * t / totalAngle;
            const float px = sliderPos + r * std::cos (t + rotation);
            const float py = cy        + r * std::sin (t + rotation);
            if (i == 0) spiral.startNewSubPath (px, py);
            else         spiral.lineTo          (px, py);
        }

        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.strokePath (spiral, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::butt));
    }
}

WaveformDisplay::WaveformDisplay (juce::AudioProcessorValueTreeState& ap,
                                   juce::AudioFormatManager& formatManager,
                                   juce::AudioThumbnailCache& cache)
    : apvts (ap),
      thumbnail (512, formatManager, cache)
{
    thumbnail.addChangeListener (this);

    searchFolderButton.onClick = [this] { onSearchFolderClicked(); };
    addChildComponent (searchFolderButton);

    addAndMakeVisible (startMarker);
    addAndMakeVisible (endMarker);

    startMarker.onPositionChanged = [this] (float p) { onStartMarkerMoved (p); };
    endMarker.onPositionChanged   = [this] (float p) { onEndMarkerMoved   (p); };

    // "Zoom" label — left end of the slider strip.
    zoomLabel.setFont (juce::Font (10.5f, juce::Font::plain));
    zoomLabel.setColour (juce::Label::textColourId,       juce::Colour (0xff909090));
    zoomLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    zoomLabel.setJustificationType (juce::Justification::centred);
    zoomLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (zoomLabel);

    // Zoom slider — sits in the kZoomSliderH strip at the bottom.
    // The strip is painted with a rainbow gradient in paint(); the slider draws
    // only a thin track stroke + thumb on top, so the background shows through.
    zoomSlider.setSliderStyle  (juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    zoomSlider.setRange        (1.0, 8.0, 0.01);
    zoomSlider.setValue        (1.0, juce::dontSendNotification);
    zoomSlider.setColour (juce::Slider::thumbColourId,      juce::Colour (0xff606060));
    zoomSlider.setColour (juce::Slider::trackColourId,      juce::Colours::white.withAlpha (0.75f));
    zoomSlider.setColour (juce::Slider::backgroundColourId, juce::Colours::black.withAlpha (0.25f));
    zoomSlider.setLookAndFeel (&zoomSliderLAF);
    zoomSlider.setPaintingIsUnclipped (true);   // allows spiral to overflow the strip
    zoomSlider.onValueChange = [this]
    {
        // Compute how far the normalised value moved since the last change and
        // accumulate that as extra spiral rotation — faster movement = more spin.
        const float currentNorm = static_cast<float> (
            (zoomSlider.getValue() - zoomSlider.getMinimum()) /
            (zoomSlider.getMaximum() - zoomSlider.getMinimum()));
        const float delta = currentNorm - zoomPrevNorm;
        zoomPrevNorm = currentNorm;
        // Scale so a full-range sweep adds roughly 5 extra turns.
        zoomSliderLAF.velocityRotation += delta * 5.0f * juce::MathConstants<float>::twoPi;
        updateViewWindow();
        repaint();
    };
    // Bold + dark label while the user holds the thumb; dim + plain when released.
    zoomSlider.onDragStart = [this]
    {
        zoomLabel.setFont   (juce::Font (10.5f, juce::Font::bold));
        zoomLabel.setColour (juce::Label::textColourId, juce::Colour (0xff101010));
    };
    zoomSlider.onDragEnd = [this]
    {
        zoomLabel.setFont   (juce::Font (10.5f, juce::Font::plain));
        zoomLabel.setColour (juce::Label::textColourId, juce::Colour (0xff909090));
    };
    addAndMakeVisible (zoomSlider);

    // BadgeOverlay is added AFTER the markers so it paints on top of them.
    addAndMakeVisible (badgeOverlay);

    startTimerHz (30);
}

WaveformDisplay::~WaveformDisplay()
{
    zoomSlider.setLookAndFeel (nullptr);  // clear before zoomSliderLAF destructs
    stopTimer();
    thumbnail.removeChangeListener (this);
}

void WaveformDisplay::setPresetName (const juce::String& name)
{
    presetName = name;
    badgeOverlay.repaint();
}

juce::String WaveformDisplay::getDisplaySampleName() const
{
    return loadedFileName;
}

void WaveformDisplay::setFile (const juce::File& file)
{
    missingFile     = false;
    missingFileName = {};
    searchStatus    = {};
    presetName      = {};
    searchFolderButton.setVisible (false);
    thumbnail.setSource (new juce::FileInputSource (file));
    loadedFileName = file.getFileName();
    loadedFile     = file;

    // Reset zoom to show the full sample on load.
    zoomSlider.setValue (1.0, juce::dontSendNotification);
    viewStart = 0.0f;
    viewWidth = 1.0f;
    startMarker.setViewWindow (0.0f, 1.0f);
    endMarker.setViewWindow   (0.0f, 1.0f);

    badgeOverlay.repaint();
    repaint();
}

void WaveformDisplay::setMissingSample (const juce::String& filename)
{
    missingFile     = true;
    missingFileName = filename;
    searchStatus    = {};
    searchFolderButton.setVisible (true);
    resized();   // re-position button for current bounds
    repaint();
}

void WaveformDisplay::setEngine (SamplerEngine* e)
{
    engine = e;
}

void WaveformDisplay::setGranulizer (GranulizerProcessor* g)
{
    granulizer = g;
}

void WaveformDisplay::syncMarkersToParameters()
{
    const float startVal = apvts.getParameterAsValue (ParamIDs::sampleStartPos).getValue();
    const float endVal   = apvts.getParameterAsValue (ParamIDs::sampleEndPos).getValue();

    startMarker.maxPos = endVal   - kMarkerEpsilon;
    endMarker.minPos   = startVal + kMarkerEpsilon;

    // Always propagate the current view window before positioning so pixel
    // placement is accurate at any zoom level.
    startMarker.setViewWindow (viewStart, viewWidth);
    endMarker.setViewWindow   (viewStart, viewWidth);

    startMarker.setNormalisedPosition (startVal);
    endMarker.setNormalisedPosition   (endVal);
}

void WaveformDisplay::resized()
{
    const int waveH = getHeight() - kZoomSliderH;
    const int width = getWidth();

    // Zoom label + slider share the bottom strip.
    zoomLabel .setBounds (0,           waveH, kZoomLabelW,         kZoomSliderH);
    // Slider is 4 px shorter than the strip and centred vertically within it.
    constexpr int sliderH = kZoomSliderH - 4;
    constexpr int kSliderInsetL = 10;  // gap between Zoom label and the slider track start
    zoomSlider.setBounds (kZoomLabelW + kSliderInsetL, waveH + (kZoomSliderH - sliderH) / 2,
                          width - kZoomLabelW - kSliderInsetL, sliderH);

    // Overlay covers the full component so it can paint the badge anywhere in the waveform area.
    badgeOverlay.setBounds (getLocalBounds());

    // Markers span only the waveform area (above the slider).
    startMarker.setSize (WaveformMarker::kWidth, waveH);
    endMarker.setSize   (WaveformMarker::kWidth, waveH);

    syncMarkersToParameters();

    // Centre the search button in the lower third of the waveform area.
    constexpr int btnW = 160;
    constexpr int btnH = 22;
    searchFolderButton.setBounds ((width - btnW) / 2,
                                  juce::roundToInt (waveH * 0.72f),
                                  btnW, btnH);
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    // White background for the whole component (slider strip included).
    g.fillAll (juce::Colours::white);

    // The waveform occupies everything above the zoom slider strip.
    const auto waveBounds    = getLocalBounds().withTrimmedBottom (kZoomSliderH).toFloat();
    const auto waveBoundsInt = waveBounds.toNearestInt();
    const float W = waveBounds.getWidth();
    const float H = waveBounds.getHeight();

    if (thumbnail.getTotalLength() > 0.0)
    {
        // Visible time window in seconds.
        const double totalLen = thumbnail.getTotalLength();
        const double visStart = static_cast<double> (viewStart) * totalLen;
        const double visEnd   = static_cast<double> (viewStart + viewWidth) * totalLen;

        // Vertical zoom: reflect normalize gain so the waveform visually scales.
        const bool normOn = apvts.getRawParameterValue (ParamIDs::normalizeEnabled)->load() > 0.5f;
        const float vZoom = (normOn && normalizeGainSrc != nullptr)
                                ? normalizeGainSrc->load (std::memory_order_relaxed)
                                : 1.0f;

        // Marker pixel positions within the waveform area, clamped so clip
        // regions stay valid even when a marker is outside the current view.
        const float startX = juce::jlimit (0.0f, W, normToPixelX (startMarker.getNormalisedPosition(), W));
        const float endX   = juce::jlimit (0.0f, W, normToPixelX (endMarker.getNormalisedPosition(),   W));

        // Inactive regions — waveform in a desaturated gray-blue.
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (0, 0, (int) startX, (int) H);
            g.setColour (juce::Colour (0xff8898a8));
            thumbnail.drawChannels (g, waveBoundsInt, visStart, visEnd, vZoom);
        }
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion ((int) endX, 0, (int) W - (int) endX, (int) H);
            g.setColour (juce::Colour (0xff8898a8));
            thumbnail.drawChannels (g, waveBoundsInt, visStart, visEnd, vZoom);
        }

        // Active region — waveform in Finnish flag blue.
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion ((int) startX, 0, (int) (endX - startX), (int) H);
            g.setColour (juce::Colour (0xff003580));
            thumbnail.drawChannels (g, waveBoundsInt, visStart, visEnd, vZoom);
        }

        // Active region: subtle light-from-above effect to lift the visible waveform.
        {
            const auto activeR = juce::Rectangle<float> (startX, 0.0f, endX - startX, H);
            if (activeR.getWidth() > 0.0f)
            {
                // Warm top-light so the blue waveform feels illuminated.
                juce::ColourGradient activeGlow (juce::Colours::white.withAlpha (0.38f), activeR.getX(), activeR.getY(),
                                                 juce::Colours::white.withAlpha (0.0f),  activeR.getX(), activeR.getY() + activeR.getHeight() * 0.6f,
                                                 false);
                g.setGradientFill (activeGlow);
                g.fillRect (activeR);

                // Thin bright line at top — acts as a specular cap.
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawHorizontalLine (juce::roundToInt (activeR.getY()), activeR.getX(), activeR.getRight());
            }
        }

        // Inactive regions — washed overlay to bleach and soften the gray waveform.
        auto paintWashRegion = [&] (juce::Rectangle<float> r)
        {
            if (r.getWidth() <= 0.0f) return;

            // Heavy white wash — desaturates what's beneath.
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillRect (r);

            // Vertical fade: brighter at top, trailing off — gives an overexposed look.
            juce::ColourGradient wash (juce::Colours::white.withAlpha (0.35f), r.getX(), r.getY(),
                                       juce::Colours::white.withAlpha (0.0f),  r.getX(), r.getY() + r.getHeight() * 0.7f,
                                       false);
            g.setGradientFill (wash);
            g.fillRect (r);

            // Thin white cap at top edge.
            g.setColour (juce::Colours::white.withAlpha (0.80f));
            g.drawHorizontalLine (juce::roundToInt (r.getY()), r.getX(), r.getRight());
        };

        paintWashRegion ({ 0.0f, 0.0f, startX, H });
        paintWashRegion ({ endX, 0.0f, W - endX, H });

        // Playback marker.
        if (cachedPlaybackPos >= 0.0f)
        {
            const float markerX = normToPixelX (cachedPlaybackPos, W);
            g.setColour (juce::Colour (0xff4dc8ff));
            g.drawLine (markerX, 0.0f, markerX, H, 2.0f);
        }

        // Granulizer grain read position marker (violet, dashed — matches granulizer knob pointers).
        if (cachedGranPos >= 0.0f)
        {
            const float granX = normToPixelX (cachedGranPos, W);
            g.setColour (juce::Colour (0xffb84dff));
            constexpr float dashLen = 6.0f;
            constexpr float gapLen  = 4.0f;
            float y = 0.0f;
            while (y < H)
            {
                const float segEnd = juce::jmin (y + dashLen, H);
                g.drawLine (granX, y, granX, segEnd, 2.0f);
                y = segEnd + gapLen;
            }
        }

        // Slicer markers — golden vertical lines at each slice boundary.
        // Read directly from APVTS so paint() always reflects current state
        // regardless of the timer update cycle.
        {
            const bool slicerOn = apvts.getRawParameterValue (ParamIDs::slicerEnabled)->load() > 0.5f;
            if (slicerOn)
            {
                // sliceCount parameter is 0-indexed; add 1 to get actual count.
                const int   numSlices = static_cast<int> (apvts.getRawParameterValue (ParamIDs::sliceCount)->load()) + 1;
                const float sStartX   = juce::jlimit (0.0f, W, normToPixelX (startMarker.getNormalisedPosition(), W));
                const float sEndX     = juce::jlimit (0.0f, W, normToPixelX (endMarker.getNormalisedPosition(),   W));
                const float sliceW    = (sEndX - sStartX) / static_cast<float> (numSlices);

                g.setColour (juce::Colour (0xffffd700));
                for (int i = 0; i <= numSlices; ++i)
                {
                    const float x = sStartX + static_cast<float> (i) * sliceW;
                    g.drawLine (x, 0.0f, x, H, 2.0f);
                }
            }
        }
    }
    else if (missingFile)
    {
        // Red warning: the project references a sample that is no longer on disk.
        g.setColour (juce::Colour (0xffcc2200));
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText ("Sample missing:  " + missingFileName,
                    waveBounds.toNearestInt(), juce::Justification::centred);

        g.setFont (juce::Font (11.0f));
        g.setColour (juce::Colours::grey);
        g.drawText ("Drop a replacement file here or use Load Sample",
                    waveBounds.withTrimmedTop (H * 0.52f).toNearestInt(),
                    juce::Justification::centred);

        // Show result of the last folder search, if any.
        if (searchStatus.isNotEmpty())
        {
            g.setColour (juce::Colour (0xffcc2200));
            g.drawText (searchStatus,
                        waveBounds.withTrimmedTop (H * 0.88f).toNearestInt(),
                        juce::Justification::centred);
        }
    }
    else
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Drop audio file here or use Load Sample",
                    waveBounds.toNearestInt(), juce::Justification::centred);
    }

    // Rainbow gradient in the zoom slider strip — ping-pong cycles so the
    // pattern looks endless (forward: red→violet, backward: violet→red, repeat).
    {
        constexpr int   kCycles = 9;
        constexpr float kAlpha  = 0.50f;   // semi-transparent against the white base

        // Seven hues in forward order.
        const juce::Colour fwd[7] = {
            juce::Colours::red.withAlpha         (kAlpha),
            juce::Colours::orange.withAlpha       (kAlpha),
            juce::Colours::yellow.withAlpha       (kAlpha),
            juce::Colours::lime.withAlpha         (kAlpha),
            juce::Colour (0xff0055ff).withAlpha   (kAlpha),   // bright blue
            juce::Colour (0xff6600cc).withAlpha   (kAlpha),   // indigo
            juce::Colour (0xffee82ee).withAlpha   (kAlpha),   // violet
        };

        const float stripY = H;
        const float stripW = static_cast<float> (getWidth());
        const float stripH = static_cast<float> (kZoomSliderH);
        const float cycleW = stripW / static_cast<float> (kCycles);

        for (int i = 0; i < kCycles; ++i)
        {
            const float x0      = i * cycleW;
            const bool  reverse = (i % 2 == 1);

            // Odd cycles run backward (violet→red) so each boundary is seamless.
            juce::ColourGradient cyc (reverse ? fwd[6] : fwd[0], x0,          stripY,
                                      reverse ? fwd[0] : fwd[6], x0 + cycleW, stripY,
                                      false);
            for (int s = 1; s <= 5; ++s)
                cyc.addColour (s / 6.0, fwd[reverse ? (6 - s) : s]);

            g.setGradientFill (cyc);
            g.fillRect (x0, stripY, cycleW, stripH);
        }
    }

    // Border covering the full component.
    g.setColour (juce::Colours::darkgrey.withAlpha (0.5f));
    g.drawRect (getLocalBounds().toFloat());
}

bool WaveformDisplay::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const juce::File file (f);
        const auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff")
            return true;
    }
    return false;
}

void WaveformDisplay::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
    {
        const juce::File file (f);
        if (file.existsAsFile())
        {
            if (onFileDrop)
                onFileDrop (file);
            return;
        }
    }
}

void WaveformDisplay::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void WaveformDisplay::timerCallback()
{
    // Decay the velocity-based spiral rotation each frame (~150 ms to settle).
    // Repainting only when there is still meaningful velocity avoids needless work.
    if (std::abs (zoomSliderLAF.velocityRotation) > 0.001f)
    {
        zoomSliderLAF.velocityRotation *= 0.75f;
        zoomSlider.repaint();
    }

    // Poll start/end params so automation moves the markers without requiring
    // a manual interaction or resize event.
    const float startVal = static_cast<float> (
        apvts.getParameterAsValue (ParamIDs::sampleStartPos).getValue());
    const float endVal = static_cast<float> (
        apvts.getParameterAsValue (ParamIDs::sampleEndPos).getValue());

    // Skip repositioning markers while the user is dragging one — the drag
    // handler already keeps the component in sync, and calling setTopLeftPosition()
    // here causes a 1-pixel stutter from the float→int round-trip.
    const bool dragging = startMarker.isMouseButtonDown() || endMarker.isMouseButtonDown();

    if (startVal != cachedStartPos || endVal != cachedEndPos)
    {
        cachedStartPos = startVal;
        cachedEndPos   = endVal;
        if (!dragging)
            syncMarkersToParameters();
        repaint();
    }

    // Poll slicer state — repaint when mode, start key, or slice count changes.
    // Also force a repaint whenever slicer is enabled so the first activation
    // always triggers a paint even if no other parameter changed.
    const bool slicerOn  = apvts.getRawParameterValue (ParamIDs::slicerEnabled)->load() > 0.5f;
    const int  loNote    = static_cast<int> (apvts.getRawParameterValue (ParamIDs::slicerLoNote)->load());
    const int  numSlices = static_cast<int> (apvts.getRawParameterValue (ParamIDs::sliceCount)->load()) + 1;

    if (slicerOn != cachedSlicerEnabled || loNote != cachedSlicerLoNote || numSlices != cachedSliceCount || slicerOn)
    {
        cachedSlicerEnabled = slicerOn;
        cachedSlicerLoNote  = loNote;
        cachedSliceCount    = numSlices;
        repaint();
    }

    // Poll normalize toggle — repaint waveform so vertical zoom reflects the gain.
    {
        const bool normOn = apvts.getRawParameterValue (ParamIDs::normalizeEnabled)->load() > 0.5f;
        if (normOn != cachedNormEnabled)
        {
            cachedNormEnabled = normOn;
            repaint();
            badgeOverlay.repaint();
        }
    }

    if (engine == nullptr)
        return;

    const float pos = engine->playbackPosition.load (std::memory_order_relaxed);
    if (pos != cachedPlaybackPos)
    {
        cachedPlaybackPos = pos;
        repaint();
    }


    // Granulizer grain read-position marker: convert the time-offset behind
    // the write head into a normalised sample position within [start, end].
    if (granulizer != nullptr)
    {
        const float offsetSec = granulizer->grainOffsetSeconds.load (std::memory_order_relaxed);
        float granPos = -1.0f;
        const double totalLen = thumbnail.getTotalLength();
        if (offsetSec >= 0.0f && cachedPlaybackPos >= 0.0f && totalLen > 0.0)
        {
            const float startPos = apvts.getRawParameterValue (ParamIDs::sampleStartPos)->load();
            const float endPos   = apvts.getRawParameterValue (ParamIDs::sampleEndPos)->load();
            const float regionLen = endPos - startPos;

            // The playback covers [startPos, endPos] over (regionLen * totalLen) seconds.
            // Convert time offset to normalised offset within the full sample.
            const float offsetNorm = static_cast<float> (offsetSec / totalLen);
            granPos = cachedPlaybackPos - offsetNorm;

            // Wrap within the [startPos, endPos] window for looping samples.
            if (granPos < startPos && regionLen > 0.0f)
                granPos = startPos + std::fmod (granPos - startPos + regionLen * 100.0f, regionLen);

            granPos = juce::jlimit (startPos, endPos, granPos);
        }
        if (granPos != cachedGranPos)
        {
            cachedGranPos = granPos;
            repaint();
        }
    }
}

void WaveformDisplay::onStartMarkerMoved (float normPos)
{
    const float endVal = static_cast<float> (
        apvts.getParameterAsValue (ParamIDs::sampleEndPos).getValue());

    const float clamped = juce::jlimit (0.0f, endVal - kMarkerEpsilon, normPos);

    if (auto* param = apvts.getParameter (ParamIDs::sampleStartPos))
        param->setValueNotifyingHost (clamped);

    endMarker.minPos = clamped + kMarkerEpsilon;
}

void WaveformDisplay::onSearchFolderClicked()
{
    folderChooser = std::make_shared<juce::FileChooser> (
        "Search for \"" + missingFileName + "\"",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory));

    folderChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto results = fc.getResults();
            if (results.isEmpty())
                return;

            const juce::File folder = results[0];
            juce::Array<juce::File> found;
            folder.findChildFiles (found, juce::File::findFiles, true, missingFileName);

            if (found.isEmpty())
            {
                searchStatus = "Not found in that folder";
                repaint();
            }
            else
            {
                // Found — load via the same path as a drag-and-drop.
                if (onFileDrop)
                    onFileDrop (found[0]);
            }
        });
}

void WaveformDisplay::onEndMarkerMoved (float normPos)
{
    const float startVal = static_cast<float> (
        apvts.getParameterAsValue (ParamIDs::sampleStartPos).getValue());

    const float clamped = juce::jlimit (startVal + kMarkerEpsilon, 1.0f, normPos);

    if (auto* param = apvts.getParameter (ParamIDs::sampleEndPos))
        param->setValueNotifyingHost (clamped);

    startMarker.maxPos = clamped - kMarkerEpsilon;
}

void WaveformDisplay::updateViewWindow()
{
    const float zoom = static_cast<float> (zoomSlider.getValue());
    viewWidth = 1.0f / zoom;

    // Centre the view on the midpoint of the current selection.
    const float mid = (startMarker.getNormalisedPosition() + endMarker.getNormalisedPosition()) * 0.5f;
    viewStart = juce::jlimit (0.0f, 1.0f - viewWidth, mid - viewWidth * 0.5f);

    // Propagate to markers and reposition them in the new view.
    startMarker.setViewWindow (viewStart, viewWidth);
    endMarker.setViewWindow   (viewStart, viewWidth);
    startMarker.setNormalisedPosition (startMarker.getNormalisedPosition());
    endMarker.setNormalisedPosition   (endMarker.getNormalisedPosition());
}

float WaveformDisplay::normToPixelX (float norm, float areaWidth) const
{
    return (norm - viewStart) / viewWidth * areaWidth;
}
