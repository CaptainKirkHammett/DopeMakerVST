#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Custom LookAndFeel for SuomiSampler.
// Knobs: dark-brown body, concentric inner rings, vintage notches, neon-green pointer.
// Colour scheme: light-blue background, purple headers/borders/highlights.
class SuomiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Sizing constants shared across all panels.
    static constexpr int kStdKnobSize    = 61;   // std rotary body
    static constexpr int kMagixxKnobSize = 72;   // Magixx rotary body — larger than std (61) but compact
    static constexpr int kTextBoxH       = 14;   // textbox height used with TextBoxBelow

    // Palette
    static const juce::Colour kBackground;
    static const juce::Colour kPurple;
    static const juce::Colour kPurpleLight;
    static const juce::Colour kPurpleDark;
    static const juce::Colour kKnobBody;
    static const juce::Colour kKnobRing;
    static const juce::Colour kKnobPointer;          // neon green    — all knobs except FX
    static const juce::Colour kKnobPointerFx;        // neon red      — FX panel knobs
    static const juce::Colour kKnobPointerEnv;       // electric blue — envelope follower knobs
    static const juce::Colour kKnobPointerGran;      // violet        — Granulizer knobs
    static const juce::Colour kKnobPointerStutter;   // vivid orange  — Stutter knobs
    static const juce::Colour kModIndicatorFx;  // wine red    — STEP/ENV mod overlay indicators
    static const juce::Colour kYellow;          // warm golden yellow — FX enable buttons ON
    static const juce::Colour kYellowDark;      // dark amber         — FX enable buttons OFF

    SuomiLookAndFeel();

    // ── Rotary knobs ──────────────────────────────────────────────────────────
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    // ── Linear sliders ────────────────────────────────────────────────────────
    // Sliders with the "darkTrack" property get a darker track; the thumb is
    // always drawn identically to the LookAndFeel_V4 default.
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    // ── Combo boxes ───────────────────────────────────────────────────────────
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawComboBoxTextWhenNothingSelected (juce::Graphics&, juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    // ── Pop-up menus ──────────────────────────────────────────────────────────
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColourToUse) override;

    // ── Buttons ───────────────────────────────────────────────────────────────
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    // ── Labels ────────────────────────────────────────────────────────────────
    juce::Font getLabelFont (juce::Label&) override;

    // Regular and bold variants of the standard label font — for direct paint() use.
    juce::Font getLabelFontRegular (float size) const { return shareFont (size); }
    juce::Font getLabelFontBold    (float size) const { return shareFontBold (size); }

private:
    juce::Typeface::Ptr shareTechMono;
    juce::Typeface::Ptr shareTechMonoBold;

    juce::Font shareFont     (float size) const;
    juce::Font shareFontBold (float size) const;
};
