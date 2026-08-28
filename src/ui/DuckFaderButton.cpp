#include "DuckFaderButton.h"
#include "SuomiLookAndFeel.h"

DuckFaderButton::DuckFaderButton (juce::AudioProcessorValueTreeState& apvts,
                                   const juce::String& paramId,
                                   const juce::String& label)
    : labelText (label)
{
    hiddenSlider.setRange (0.0, 1.0);
    hiddenSlider.addListener (this);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, paramId, hiddenSlider);
    setRepaintsOnMouseActivity (true);
}

void DuckFaderButton::paint (juce::Graphics& g)
{
    const float value  = static_cast<float> (hiddenSlider.getValue());
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    // Dark background (disabled tint).
    g.setColour (SuomiLookAndFeel::kPurpleDark.withAlpha (0.75f));
    g.fillRoundedRectangle (bounds, 3.0f);

    // Bright fill rising from the bottom.
    // Use fully-opaque kPurple so the fill matches the toggle-button ON brightness
    // regardless of the dark base underneath.
    if (value > 0.0f)
    {
        const float fillH = bounds.getHeight() * value;
        auto fillRect = bounds.withTop (bounds.getBottom() - fillH);

        juce::Path fillPath;
        fillPath.addRoundedRectangle (bounds.getX(), bounds.getY(),
                                      bounds.getWidth(), bounds.getHeight(),
                                      3.0f, 3.0f,
                                      false, false,   // top corners — not rounded at fill boundary
                                      true,  true);    // bottom corners stay rounded
        g.saveState();
        g.reduceClipRegion (fillRect.toNearestInt());
        g.setColour (SuomiLookAndFeel::kPurple);      // fully opaque — no dark bleed-through
        g.fillPath (fillPath);
        g.restoreState();
    }

    // Border — brighter when hovered.
    const bool isHovered = isMouseOver();
    g.setColour (isHovered ? SuomiLookAndFeel::kPurpleLight
                           : SuomiLookAndFeel::kPurple.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // Hint arrows: two small up-triangles flanking the label, shown only while the
    // button is pressed AND the value is still at zero (disappear once dragged up).
    if (isPressed && value <= 0.0f)
    {
        const float cy = bounds.getCentreY();
        const float aw = 6.0f;   // arrow base width
        const float ah = 5.0f;   // arrow height

        auto drawUpArrow = [&] (float cx)
        {
            juce::Path arrow;
            arrow.startNewSubPath (cx,          cy - ah * 0.5f);   // tip
            arrow.lineTo          (cx - aw * 0.5f, cy + ah * 0.5f);  // bottom-left
            arrow.lineTo          (cx + aw * 0.5f, cy + ah * 0.5f);  // bottom-right
            arrow.closeSubPath();
            g.fillPath (arrow);
        };

        g.setColour (juce::Colours::white.withAlpha (0.75f));
        drawUpArrow (bounds.getX() + 7.0f);                // left of text
        drawUpArrow (bounds.getRight() - 7.0f);            // right of text
    }

    // Label — full white as soon as value > 0, dim white when fully off.
    g.setColour (value > 0.0f ? juce::Colours::white : juce::Colours::white.withAlpha (0.55f));
    if (auto* laf = dynamic_cast<SuomiLookAndFeel*> (&getLookAndFeel()))
        g.setFont (laf->getLabelFontRegular (17.60f));
    else
        g.setFont (17.60f);
    g.drawText (labelText, getLocalBounds(), juce::Justification::centred, false);
}

void DuckFaderButton::mouseDown (const juce::MouseEvent& e)
{
    dragStartValue = static_cast<float> (hiddenSlider.getValue());
    dragStartY     = e.getPosition().getY();
    isPressed      = true;
    repaint();
}

void DuckFaderButton::mouseDrag (const juce::MouseEvent& e)
{
    const float delta    = static_cast<float> (dragStartY - e.getPosition().getY());
    const float newValue = juce::jlimit (0.0f, 1.0f, dragStartValue + delta / kDragRange);
    hiddenSlider.setValue (static_cast<double> (newValue),
                           juce::sendNotificationAsync);
}

void DuckFaderButton::mouseUp (const juce::MouseEvent&)
{
    isPressed = false;
    repaint();
}

void DuckFaderButton::mouseWheelMove (const juce::MouseEvent&,
                                       const juce::MouseWheelDetails& wheel)
{
    const float current  = static_cast<float> (hiddenSlider.getValue());
    const float newValue = juce::jlimit (0.0f, 1.0f, current + wheel.deltaY * 0.1f);
    hiddenSlider.setValue (static_cast<double> (newValue),
                           juce::sendNotificationAsync);
}

void DuckFaderButton::mouseDoubleClick (const juce::MouseEvent&)
{
    // Double-click resets to off.
    hiddenSlider.setValue (0.0, juce::sendNotificationAsync);
}

void DuckFaderButton::sliderValueChanged (juce::Slider*)
{
    repaint();
}
