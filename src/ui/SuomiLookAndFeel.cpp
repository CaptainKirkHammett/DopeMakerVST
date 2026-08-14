#include "SuomiLookAndFeel.h"
#include <BinaryData.h>

const juce::Colour SuomiLookAndFeel::kBackground  { 0xffa8cfe0 };  // soft light blue
const juce::Colour SuomiLookAndFeel::kPurple       { 0xff7b3fbf };  // mid purple
const juce::Colour SuomiLookAndFeel::kPurpleLight  { 0xffb06de0 };  // lighter purple
const juce::Colour SuomiLookAndFeel::kPurpleDark   { 0xff280044 };  // deep purple
const juce::Colour SuomiLookAndFeel::kKnobBody     { 0xff3d1a02 };  // dark brown
const juce::Colour SuomiLookAndFeel::kKnobRing     { 0xff6b3510 };  // lighter brown ring
const juce::Colour SuomiLookAndFeel::kKnobPointer        { 0xff39ff14 };  // neon green
const juce::Colour SuomiLookAndFeel::kKnobPointerFx      { 0xffff1744 };  // neon red      (FX panels)
const juce::Colour SuomiLookAndFeel::kKnobPointerEnv     { 0xff00b4ff };  // electric blue (env follower)
const juce::Colour SuomiLookAndFeel::kKnobPointerGran    { 0xffb84dff };  // violet        (Granulizer)
const juce::Colour SuomiLookAndFeel::kKnobPointerStutter { 0xffff9500 };  // vivid orange  (Stutter)
const juce::Colour SuomiLookAndFeel::kModIndicatorFx { 0xffc02040 }; // vivid wine red  (STEP/ENV mod indicators)
const juce::Colour SuomiLookAndFeel::kYellow     { 0xffcc9900 };   // warm golden yellow
const juce::Colour SuomiLookAndFeel::kYellowDark  { 0xff5a4200 };   // dark amber

juce::Font SuomiLookAndFeel::shareFont (float size) const
{
    return juce::Font (shareTechMono).withHeight (size);
}

juce::Font SuomiLookAndFeel::shareFontBold (float size) const
{
    return juce::Font (shareTechMonoBold).withHeight (size);
}

SuomiLookAndFeel::SuomiLookAndFeel()
{
    shareTechMono = juce::Typeface::createSystemTypefaceFor (
        BinaryData::Inconsolata_ttf, BinaryData::Inconsolata_ttfSize);
    shareTechMonoBold = juce::Typeface::createSystemTypefaceFor (
        BinaryData::InconsolataBold_ttf, BinaryData::InconsolataBold_ttfSize);
    setColour (juce::ResizableWindow::backgroundColourId,    kBackground);
    setColour (juce::Label::textColourId,                    juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId,            juce::Colours::white);
    setColour (juce::Slider::textBoxBackgroundColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,         juce::Colours::transparentBlack);
    setColour (juce::Slider::thumbColourId,                  kPurpleLight);
    setColour (juce::Slider::trackColourId,                  kPurple.withAlpha (0.5f));
    setColour (juce::ComboBox::backgroundColourId,           kPurpleDark);
    setColour (juce::ComboBox::textColourId,                 juce::Colours::white);
    setColour (juce::ComboBox::arrowColourId,                kPurpleLight);
    setColour (juce::ComboBox::outlineColourId,              kPurple.withAlpha (0.6f));
    setColour (juce::PopupMenu::backgroundColourId,          kPurpleDark);
    setColour (juce::PopupMenu::textColourId,                juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kPurple);
    setColour (juce::PopupMenu::highlightedTextColourId,     juce::Colours::white);
    setColour (juce::ToggleButton::textColourId,             juce::Colours::white);
    setColour (juce::ToggleButton::tickColourId,             kPurpleLight);
    setColour (juce::ToggleButton::tickDisabledColourId,     kPurple.withAlpha (0.35f));
    setColour (juce::TextButton::buttonColourId,             kPurpleDark);
    setColour (juce::TextButton::buttonOnColourId,           kPurple);
    setColour (juce::TextButton::textColourOffId,            juce::Colours::white);
    setColour (juce::TextButton::textColourOnId,             juce::Colours::white);
}

//==============================================================================
void SuomiLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPosProportional,
                                          float rotaryStartAngle, float rotaryEndAngle,
                                          juce::Slider& slider)
{
    // JUCE already passes the textbox-excluded bounds here — draw in the full provided rect.
    const float radius = juce::jmin (width / 2.0f, height / 2.0f) - 4.0f;
    const float cx     = x + width  * 0.5f;
    const float cy     = y + height * 0.5f;

    if (radius < 4.0f)
        return;

    // FX-section knobs carry fxKnob, gateKnob, or envKnob — thicker rim ticks.
    const bool isFxSection = static_cast<bool> (slider.getProperties()["fxKnob"])
                           || static_cast<bool> (slider.getProperties()["gateKnob"])
                           || static_cast<bool> (slider.getProperties()["envKnob"]);
    const float tickScale = isFxSection ? 1.8f : 1.0f;

    // ── Outer tick marks ──────────────────────────────────────────────────────
    {
        const int numTicks = 13;
        for (int i = 0; i <= numTicks; ++i)
        {
            const float t     = static_cast<float> (i) / numTicks;
            const float angle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle)
                                - juce::MathConstants<float>::halfPi;
            const float ca    = std::cos (angle);
            const float sa    = std::sin (angle);
            // Long tick every 3rd, short elsewhere.
            const bool  isLong = (i % 3 == 0);
            const float r0 = radius + 3.0f;
            const float r1 = radius + (isLong ? 8.0f : 5.0f);
            g.setColour (juce::Colours::white.withAlpha (isLong ? 0.75f : 0.40f));
            g.drawLine (cx + ca * r0, cy + sa * r0,
                        cx + ca * r1, cy + sa * r1,
                        (isLong ? 1.5f : 1.0f) * tickScale);
        }
    }

    // ── Knob body — radial gradient (dark purple for gain knobs, dark brown otherwise)
    {
        const bool isGain = static_cast<bool> (slider.getProperties()["gainKnob"]);
        const juce::Colour body = isGain ? juce::Colour (0xff2a1540) : kKnobBody;
        juce::ColourGradient grad (body.brighter (0.22f),
                                   cx - radius * 0.35f, cy - radius * 0.45f,
                                   body.darker  (0.35f),
                                   cx + radius * 0.35f, cy + radius * 0.45f,
                                   false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }

    // ── Notches around the rim (vintage detent look) ──────────────────────────
    {
        const int numNotches = 28;
        for (int i = 0; i < numNotches; ++i)
        {
            const float a  = static_cast<float> (i) / numNotches
                             * juce::MathConstants<float>::twoPi
                             - juce::MathConstants<float>::halfPi;
            const float ca = std::cos (a);
            const float sa = std::sin (a);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawLine (cx + ca * (radius - 4.0f), cy + sa * (radius - 4.0f),
                        cx + ca *  radius,          cy + sa *  radius, 1.6f);
        }
    }

    // ── Outer rim with purple tint ────────────────────────────────────────────
    g.setColour (kPurple.withAlpha (0.40f));
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.3f);

    // ── Concentric inner rings ────────────────────────────────────────────────
    {
        const float ir1 = radius * 0.70f;
        g.setColour (kKnobRing);
        g.drawEllipse (cx - ir1, cy - ir1, ir1 * 2.0f, ir1 * 2.0f, 1.8f);

        const float ir2 = radius * 0.48f;
        g.setColour (kKnobRing.withAlpha (0.55f));
        g.drawEllipse (cx - ir2, cy - ir2, ir2 * 2.0f, ir2 * 2.0f, 1.0f);
    }

    // ── Pointer line: neon green (default), neon red (fxKnob), purple (gateKnob) ──
    {
        const bool isFxKnob         = static_cast<bool> (slider.getProperties()["fxKnob"]);
        const bool isGateKnob       = static_cast<bool> (slider.getProperties()["gateKnob"]);
        const bool isEnvKnob        = static_cast<bool> (slider.getProperties()["envKnob"]);
        const bool isGranKnob       = static_cast<bool> (slider.getProperties()["granKnob"]);
        const bool isStutterKnob    = static_cast<bool> (slider.getProperties()["stutterKnob"]);
        const bool isGainKnob       = static_cast<bool> (slider.getProperties()["gainKnob"]);
        const bool isSemitoneKnob   = static_cast<bool> (slider.getProperties()["semitoneKnob"]);
        const bool isLfoDisabled    = static_cast<bool> (slider.getProperties()["lfoDisabled"]);
        const bool isThickPointer   = static_cast<bool> (slider.getProperties()["thickPointer"]);
        const bool isCentreTextKnob = slider.getProperties().contains ("centreTextKnob");

        // Limiter GR visualisation — written by SampleControlPanel timer.
        // When present, the pointer shifts green → orange → red and widens.
        const bool  hasLimiterGr = slider.getProperties().contains ("limiterGrDb");
        const float limiterGrDb  = hasLimiterGr
            ? static_cast<float> (static_cast<double> (slider.getProperties()["limiterGrDb"]))
            : 0.0f;

        juce::Colour pointerCol;
        if (hasLimiterGr)
        {
            // 4-stop gradient: neon green → yellow → orange → red.
            // 12 dB of smoothed level-above-threshold maps to full red.
            const float t = juce::jlimit (0.0f, 1.0f, limiterGrDb / 12.0f);
            if (t < 0.33f)
                pointerCol = kKnobPointer.interpolatedWith (
                    juce::Colour (0xffffff00), t / 0.33f);                            // green → yellow
            else if (t < 0.67f)
                pointerCol = juce::Colour (0xffffff00).interpolatedWith (
                    juce::Colour (0xffff6600), (t - 0.33f) / 0.34f);                 // yellow → orange
            else
                pointerCol = juce::Colour (0xffff6600).interpolatedWith (
                    juce::Colour (0xffff1100), (t - 0.67f) / 0.33f);                 // orange → red
        }
        else
        {
            pointerCol = isLfoDisabled   ? juce::Colour (0xff444444)
                       : isGainKnob    ? juce::Colour (0xffffffff)  // white
                       : isEnvKnob      ? kKnobPointerEnv
                       : isGateKnob     ? kPurpleLight
                       : isGranKnob     ? kKnobPointerGran
                       : isStutterKnob  ? kKnobPointerStutter
                       : isFxKnob       ? kKnobPointerFx
                       : isThickPointer ? juce::Colour (0xffffe000)  // bright yellow
                                        : kKnobPointer;
        }

        const float pointerAngle = rotaryStartAngle
                                   + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle)
                                   - juce::MathConstants<float>::halfPi;
        const float ca = std::cos (pointerAngle);
        const float sa = std::sin (pointerAngle);

        if (isSemitoneKnob || isCentreTextKnob)
        {
            // Fixed-width pointer tip — always shows knob position clearly.
            const float r0 = radius * 0.78f;
            const float r1 = radius * 0.97f;
            g.setColour (pointerCol.withAlpha (0.35f));
            g.drawLine (cx + ca * r0, cy + sa * r0,
                        cx + ca * r1, cy + sa * r1, 5.5f);
            g.setColour (pointerCol);
            g.drawLine (cx + ca * r0, cy + sa * r0,
                        cx + ca * r1, cy + sa * r1, 2.3f);

            // GR arc — sweeps LEFT of the pointer on the knob rim, representing
            // the amount of gain reduction applied by the limiter.  The arc starts
            // at the current pointer angle and extends counter-clockwise, so a
            // larger suppression produces a longer leftward band — mirroring how
            // LFO mod indicators show a range relative to the knob's current position.
            // Stroke style matches LfoModIndicator exactly (7 pt glow / 3 pt core).
            if (hasLimiterGr)
            {
                const float grT = juce::jlimit (0.0f, 1.0f, limiterGrDb / 12.0f);
                if (grT > 0.01f)
                {
                    const float arcEndAngle = rotaryStartAngle
                                              + sliderPosProportional
                                                * (rotaryEndAngle - rotaryStartAngle);
                    const float sweep     = grT * 0.65f;  // up to ~37° at full GR
                    const float arcRadius = radius - 1.5f;

                    juce::Path arc;
                    arc.addArc (cx - arcRadius, cy - arcRadius,
                                arcRadius * 2.0f, arcRadius * 2.0f,
                                arcEndAngle - sweep, arcEndAngle, true);

                    const juce::PathStrokeType core { 3.0f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded };
                    const juce::PathStrokeType glow { 7.0f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded };
                    g.setColour (pointerCol.withAlpha (0.30f));
                    g.strokePath (arc, glow);
                    g.setColour (pointerCol.withAlpha (0.88f));
                    g.strokePath (arc, core);
                }
            }
        }
        else
        {
            const float r0 = radius * 0.18f;   // starts just past center dot
            const float r1 = radius * 0.83f;

            // Glow
            g.setColour (pointerCol.withAlpha (0.30f));
            g.drawLine (cx + ca * r0, cy + sa * r0,
                        cx + ca * r1, cy + sa * r1, 5.5f);
            // Core
            g.setColour (pointerCol);
            g.drawLine (cx + ca * r0, cy + sa * r0,
                        cx + ca * r1, cy + sa * r1, 2.3f);

            // Small center dot
            g.setColour (pointerCol.withAlpha (0.7f));
            g.fillEllipse (cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
        }

        // Centred label drawn over the knob face.
        if (isCentreTextKnob)
        {
            const juce::String mode  = slider.getProperties()["centreTextKnob"].toString();
            const float        halfW = radius * 0.72f;
            g.setColour (juce::Colours::white.withAlpha (0.90f));

            if (mode == "octave")
            {
                const juce::String label = juce::String (static_cast<int> (std::round (slider.getValue())) + 1);
                g.setFont (shareFontBold (radius * 1.19f));
                g.drawText (label,
                            juce::Rectangle<float> (cx - halfW, cy - radius * 0.65f,
                                                    halfW * 2.0f, radius * 1.30f).toNearestInt(),
                            juce::Justification::centred, false);
            }
            else if (mode == "stageDb")
            {
                // Two-line: dB number on top, "dB" unit below.
                const int stages = juce::jlimit (1, 8, static_cast<int> (slider.getValue() * 8) + 1);
                const juce::String numStr  = juce::String (stages * 12);
                const juce::String unitStr = "dB";
                const float fh = radius * 0.72f;
                g.setFont (shareFontBold (fh));
                g.drawText (numStr,
                            juce::Rectangle<float> (cx - halfW, cy - fh, halfW * 2.0f, fh).toNearestInt(),
                            juce::Justification::centred, false);
                g.drawText (unitStr,
                            juce::Rectangle<float> (cx - halfW, cy,      halfW * 2.0f, fh).toNearestInt(),
                            juce::Justification::centred, false);
            }
            else if (mode == "dir")
            {
                juce::String label;
                switch (static_cast<int> (std::round (slider.getValue())))
                {
                    case 1:  label = "\\"; break;
                    case 2:  label = "?";  break;
                    default: label = "/";  break;
                }
                g.setFont (shareFontBold (radius * 1.19f));
                g.drawText (label,
                            juce::Rectangle<float> (cx - halfW, cy - radius * 0.65f,
                                                    halfW * 2.0f, radius * 1.30f).toNearestInt(),
                            juce::Justification::centred, false);
            }
            else if (mode == "wave")
            {
                // Maps AudioParameterChoice index 0-6 to short waveform labels.
                // Two-line labels keep each line to ≤3 chars so they fit in the knob face.
                static const char* kWaveLabels[] =
                    { "Sin", "Tri", "Sqr", "Saw\nUp", "Saw\nDn", "Rnd\n~", "Rnd\n|" };
                const int idx = juce::jlimit (0, 6, static_cast<int> (std::round (slider.getValue())));
                const juce::String waveStr = kWaveLabels[idx];
                const int wSep = waveStr.indexOf ("\n");
                if (wSep >= 0)
                {
                    const float fh = radius * 0.72f;
                    g.setFont (shareFontBold (fh));
                    g.drawText (waveStr.substring (0, wSep),
                                juce::Rectangle<float> (cx - halfW, cy - fh, halfW * 2.0f, fh).toNearestInt(),
                                juce::Justification::centred, false);
                    g.drawText (waveStr.substring (wSep + 1),
                                juce::Rectangle<float> (cx - halfW, cy, halfW * 2.0f, fh).toNearestInt(),
                                juce::Justification::centred, false);
                }
                else
                {
                    g.setFont (shareFontBold (radius * 0.97f));
                    g.drawText (waveStr,
                                juce::Rectangle<float> (cx - halfW, cy - radius * 0.55f,
                                                        halfW * 2.0f, radius * 1.10f).toNearestInt(),
                                juce::Justification::centred, false);
                }
            }
            else
            {
                // Static text — split on '\n' for two-line display.
                const int sep = mode.indexOf ("\n");
                if (sep >= 0)
                {
                    const juce::String line1 = mode.substring (0, sep);
                    const juce::String line2 = mode.substring (sep + 1);
                    const float fh = radius * 0.72f;   // 25 % bigger; rows touch (gap = 0)
                    g.setFont (shareFontBold (fh));
                    g.drawText (line1,
                                juce::Rectangle<float> (cx - halfW, cy - fh,
                                                        halfW * 2.0f, fh).toNearestInt(),
                                juce::Justification::centred, false);
                    g.drawText (line2,
                                juce::Rectangle<float> (cx - halfW, cy,
                                                        halfW * 2.0f, fh).toNearestInt(),
                                juce::Justification::centred, false);
                }
                else
                {
                    // Auto-scale font for longer labels so they fit the knob face.
                    // Labels of 3 chars use full size; each extra char shrinks proportionally.
                    const float fontH = radius * 0.97f
                                        * juce::jmin (1.0f, 3.0f / static_cast<float> (
                                              juce::jmax (1, mode.length())));
                    g.setFont (shareFontBold (fontH));
                    // Use slightly wider rect (0.90 × radius each side) to
                    // comfortably fit 4-char labels such as "Freq".
                    g.drawText (mode,
                                juce::Rectangle<float> (cx - radius * 0.90f, cy - radius * 0.55f,
                                                        radius * 1.80f, radius * 1.10f).toNearestInt(),
                                juce::Justification::centred, false);
                }
            }
        }
    }
}

//==============================================================================
void SuomiLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                      bool isButtonDown,
                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                      juce::ComboBox& box)
{
    const float w = static_cast<float> (width);
    const float h = static_cast<float> (height);

    // "toggleStyle" property: honour per-instance colours and omit the arrow,
    // so the combo renders identically to a ToggleButton.
    if (static_cast<bool> (box.getProperties()["toggleStyle"]))
    {
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (0.0f, 0.0f, w, h, 3.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 3.0f, 1.0f);
        return;   // no arrow
    }

    g.setColour (isButtonDown ? kPurpleDark.brighter (0.1f) : kPurpleDark);
    g.fillRoundedRectangle (0.0f, 0.0f, w, h, 3.0f);

    g.setColour (kPurple.withAlpha (0.75f));
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.0f, h - 1.0f, 3.0f, 1.0f);

    // Centred-text boxes (e.g. the scale selector) show no arrow.
    if (static_cast<bool> (box.getProperties()["centredText"]))
        return;

    // Down-arrow
    const float arrowCX = static_cast<float> (buttonX) + buttonW * 0.5f;
    const float arrowCY = static_cast<float> (buttonY) + buttonH * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (arrowCX - 4.5f, arrowCY - 2.0f,
                       arrowCX + 4.5f, arrowCY - 2.0f,
                       arrowCX,        arrowCY + 3.5f);
    g.setColour (kPurpleLight);
    g.fillPath (arrow);
}

void SuomiLookAndFeel::drawComboBoxTextWhenNothingSelected (juce::Graphics& g,
                                                             juce::ComboBox& box,
                                                             juce::Label& label)
{
    // Centred-text boxes use full white so the placeholder matches TextButton style.
    const bool isCentred = static_cast<bool> (box.getProperties()["centredText"]);
    g.setColour (findColour (juce::ComboBox::textColourId)
                     .withMultipliedAlpha (isCentred ? 1.0f : 0.5f));
    g.setFont (label.getLookAndFeel().getLabelFont (label));
    // Use the full label area for centred boxes — the default border is proportional
    // to font height and would push a small button's text far off-centre.
    // For centred boxes draw in the full ComboBox area so text is pixel-perfect
    // centred — label.getLocalBounds() is in label-local coords and would be 1 px off.
    auto textArea = isCentred ? box.getLocalBounds()
                              : getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
    g.drawFittedText (box.getTextWhenNothingSelected(), textArea,
                      label.getJustificationType(),
                      juce::jmax (1, (int) ((float) textArea.getHeight() / label.getLookAndFeel().getLabelFont (label).getHeight())),
                      1.0f);
}

void SuomiLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    if (static_cast<bool> (box.getProperties()["toggleStyle"]))
    {
        label.setBounds (1, 1, box.getWidth() - 2, box.getHeight() - 2);
    }
    else if (static_cast<bool> (box.getProperties()["centredText"]))
    {
        label.setBounds (1, 1, box.getWidth() - 2, box.getHeight() - 2);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colours::white);
    }
    else
    {
        label.setBounds (6, 1, box.getWidth() - 28, box.getHeight() - 2);
    }
    label.setFont (getComboBoxFont (box));
}

juce::Font SuomiLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return shareFont (19.21f);
}

//==============================================================================
void SuomiLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.setColour (kPurpleDark);
    g.fillRect (0, 0, width, height);
    g.setColour (kPurple.withAlpha (0.55f));
    g.drawRect (0, 0, width, height, 1);
}

void SuomiLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                           const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive, bool isHighlighted,
                                           bool /*isTicked*/, bool /*hasSubMenu*/,
                                           const juce::String& text,
                                           const juce::String& /*shortcutKeyText*/,
                                           const juce::Drawable* /*icon*/,
                                           const juce::Colour* /*textColourToUse*/)
{
    if (isSeparator)
    {
        g.setColour (kPurple.withAlpha (0.3f));
        g.drawHorizontalLine (area.getCentreY(), (float) area.getX(), (float) area.getRight());
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (kPurple.withAlpha (0.65f));
        g.fillRect (area);
    }

    g.setColour (isActive ? juce::Colours::white : juce::Colours::grey);
    g.setFont (shareFont (19.21f));
    g.drawText (text, area.reduced (8, 0), juce::Justification::centredLeft, true);
}

//==============================================================================
void SuomiLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool isHighlighted, bool /*isDown*/)
{
    const bool isOn      = button.getToggleState();
    const bool isFxButton = static_cast<bool> (button.getProperties()["fxButton"]);
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    if (isFxButton)
    {
        g.setColour (isOn ? kYellow.withAlpha (0.90f) : kYellowDark.withAlpha (0.80f));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (isHighlighted ? kYellow.brighter (0.3f) : kYellow.withAlpha (0.60f));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
        g.setColour (isOn ? juce::Colours::black : juce::Colours::white.withAlpha (0.70f));
    }
    else
    {
        g.setColour (isOn ? kPurple.withAlpha (0.85f) : kPurpleDark.withAlpha (0.75f));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (isHighlighted ? kPurpleLight : kPurple.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
        g.setColour (isOn ? juce::Colours::white : juce::Colours::white.withAlpha (0.55f));
    }

    const bool smallFont = static_cast<bool> (button.getProperties()["smallFont"]);
    if (isFxButton)
        g.setFont (shareFontBold (19.37f));
    else if (smallFont)
        g.setFont (shareFont (13.0f));
    else
        g.setFont (shareFont (17.60f));
    g.drawText (button.getButtonText(), button.getLocalBounds(),
                juce::Justification::centred, false);
}

void SuomiLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour& /*backgroundColour*/,
                                              bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool isOn = button.getToggleState();

    juce::Colour fill = isDown       ? kPurple.brighter (0.3f)
                        : isOn        ? kPurple
                        : isHighlighted ? kPurpleDark.brighter (0.18f)
                                        : kPurpleDark;
    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (isHighlighted ? kPurpleLight : kPurple.withAlpha (0.65f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

void SuomiLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool /*isHighlighted*/, bool /*isDown*/)
{
    g.setColour (juce::Colours::white);
    g.setFont (shareFont (19.21f));
    g.drawText (button.getButtonText(), button.getLocalBounds(),
                juce::Justification::centred, true);
}

//==============================================================================
void SuomiLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                          minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const bool isDarkTrack = static_cast<bool> (slider.getProperties()["darkTrack"]);

    const float cx         = x + width * 0.5f;
    const float trackWidth = juce::jmin (6.0f, width * 0.25f);
    const float top        = static_cast<float> (y);
    const float bottom     = static_cast<float> (y + height);

    // Background track.
    juce::Path bgPath;
    bgPath.startNewSubPath (cx, bottom);
    bgPath.lineTo          (cx, top);
    // Dark neon green for marked channels; default track colours elsewhere.
    const juce::Colour darkGreen = kKnobPointer.darker (0.55f);
    g.setColour (isDarkTrack ? darkGreen : slider.findColour (juce::Slider::backgroundColourId));
    g.strokePath (bgPath, juce::PathStrokeType (trackWidth,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    // Value fill (bottom up to thumb).
    juce::Path valPath;
    valPath.startNewSubPath (cx, bottom);
    valPath.lineTo          (cx, sliderPos);
    g.setColour (isDarkTrack ? darkGreen.brighter (0.30f)
                             : slider.findColour (juce::Slider::trackColourId));
    g.strokePath (valPath, juce::PathStrokeType (trackWidth,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    // Thumb — fixed small radius so it matches the pre-override appearance.
    const float thumbR = 5.0f;
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (juce::Rectangle<float> (thumbR * 2.0f, thumbR * 2.0f)
                       .withCentre ({ cx, sliderPos }));
}

//==============================================================================
juce::Font SuomiLookAndFeel::getLabelFont (juce::Label& label)
{
    // If the label has opted in to a custom font, honour label.getFont().
    // All other labels use the shared small size so knob captions stay compact.
    if (label.getProperties().contains ("customFont"))
    {
        const float h = label.getFont().getHeight();
        return label.getFont().isBold() ? shareFontBold (h) : shareFont (h);
    }

    return shareFont (18.46f);
}
