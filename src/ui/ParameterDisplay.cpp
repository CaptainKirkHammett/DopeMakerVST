#include "ParameterDisplay.h"

ParameterDisplay::ParameterDisplay (juce::AudioProcessorValueTreeState& a)
    : apvts (a)
{
    // Intercept clicks on this component so we can open the editor.
    setInterceptsMouseClicks (true, false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    editor.setMultiLine (false);
    editor.setReturnKeyStartsNewLine (false);
    editor.setScrollbarsShown (false);
    editor.setJustification (juce::Justification::centred);
    editor.setColour (juce::TextEditor::backgroundColourId,      juce::Colour (0xff1a1a2e));
    editor.setColour (juce::TextEditor::textColourId,            juce::Colour (0xffa0d8ff));
    editor.setColour (juce::TextEditor::highlightColourId,       juce::Colour (0x6600aaff));
    editor.setColour (juce::TextEditor::outlineColourId,         juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId,  juce::Colours::transparentBlack);
    editor.addListener (this);
    addChildComponent (editor);   // hidden until the user clicks
}

// ── Public ────────────────────────────────────────────────────────────────────

void ParameterDisplay::showParameter (const juce::String& name,
                                      const juce::String& value,
                                      const juce::String& paramId)
{
    paramName       = name;
    paramValue      = value;
    currentParamId  = paramId;

    // If the user is mid-edit when a new parameter is reported, dismiss the
    // editor rather than silently switching to a different parameter.
    if (isEditing)
        cancelEdit();

    repaint();
}

// ── juce::Component overrides ─────────────────────────────────────────────────

void ParameterDisplay::resized()
{
    // Mirror the value-text area used in paint() so the TextEditor sits
    // exactly over where the value text would be drawn.
    auto lcd  = lcdRect();
    auto valueArea = lcd.removeFromBottom (lcd.getHeight() * 0.55f);
    editor.setBounds (valueArea.toNearestInt());

    // Match font size to the painted value text.
    editor.applyFontToAllText (juce::Font (lcdRect().getHeight() * 0.50f, juce::Font::bold));
}

void ParameterDisplay::mouseUp (const juce::MouseEvent&)
{
    if (currentParamId.isEmpty())
        return;

    // Choice parameters display named options rather than numeric values;
    // free-text entry is not meaningful for them.
    if (dynamic_cast<juce::AudioParameterChoice*> (
            apvts.getParameter (currentParamId)) != nullptr)
        return;

    enterEditMode();
}

void ParameterDisplay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Pink bezel that blends the display into the FX panel row.
    g.setColour (juce::Colour (0x33ff69b4));
    g.fillRoundedRectangle (b, 4.0f);

    auto lcd = lcdRect();

    // Dark LCD background.
    g.setColour (juce::Colour (0xff1a1a2e));
    g.fillRoundedRectangle (lcd, 3.0f);

    // Cyan border to signal edit mode.
    if (isEditing)
    {
        g.setColour (juce::Colour (0xff00aaff));
        g.drawRoundedRectangle (lcd.reduced (0.5f), 3.0f, 1.5f);
    }

    const float h = lcd.getHeight();

    // Split LCD into name (top 45 %) and value (bottom 55 %) areas.
    auto valuArea = lcd;
    auto nameArea = valuArea.removeFromTop (h * 0.45f);

    // Name text — always visible, even while editing.
    g.setColour (juce::Colour (0xff6080a0));
    g.setFont (juce::Font (h * 0.38f, juce::Font::plain));
    g.drawText (paramName, nameArea, juce::Justification::centred, true);

    // Value text — hidden while the TextEditor child is active.
    if (!isEditing)
    {
        g.setColour (juce::Colour (0xffa0d8ff));
        g.setFont (juce::Font (h * 0.50f, juce::Font::bold));
        g.drawText (formattedDisplayValue(), valuArea, juce::Justification::centred, true);
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

juce::String ParameterDisplay::formattedDisplayValue() const
{
    if (paramValue == "---" || paramValue.isEmpty())
        return paramValue;

    const juce::juce_wchar first = paramValue.trimStart()[0];
    // Treat as plain number only when it doesn't look like a ratio
    // string (e.g. "1/32", "1/4D") that starts with a digit but is
    // not a simple decimal.
    const bool looksNumeric =
        (juce::CharacterFunctions::isDigit (first) || first == '-')
        && !paramValue.contains ("/");

    if (!looksNumeric)
        return paramValue;

    const int dotPos = paramValue.indexOfChar ('.');
    if (dotPos < 0)
        return juce::String (static_cast<int> (paramValue.getDoubleValue()));

    return juce::String (paramValue.getDoubleValue(),
                         juce::jmin (4, paramValue.length() - dotPos - 1));
}

juce::Rectangle<float> ParameterDisplay::lcdRect() const
{
    return getLocalBounds().toFloat().reduced (5.0f, 10.0f);
}

void ParameterDisplay::enterEditMode()
{
    isEditing = true;

    // Pre-fill with the same formatted value shown in the display so the user
    // sees the same number of decimals they observed while tweaking.
    editor.setText (formattedDisplayValue(), false);
    editor.setVisible (true);
    editor.selectAll();
    editor.grabKeyboardFocus();
    repaint();
}

void ParameterDisplay::commitEdit()
{
    auto* param = dynamic_cast<juce::RangedAudioParameter*> (
        apvts.getParameter (currentParamId));

    if (param != nullptr)
    {
        // getValueForText() converts the typed text to a normalised [0..1]
        // value using the parameter's own text-to-value conversion, which
        // handles unit suffixes and custom converters automatically.
        const float normalised = param->getValueForText (editor.getText());
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
    }

    cancelEdit();
}

void ParameterDisplay::cancelEdit()
{
    if (!isEditing)
        return;  // guard against re-entry (e.g. focus-lost after commitEdit)

    isEditing = false;
    editor.setVisible (false);
    repaint();
}

// ── TextEditor::Listener ──────────────────────────────────────────────────────

void ParameterDisplay::textEditorReturnKeyPressed (juce::TextEditor&) { commitEdit(); }
void ParameterDisplay::textEditorEscapeKeyPressed (juce::TextEditor&) { cancelEdit(); }
void ParameterDisplay::textEditorFocusLost        (juce::TextEditor&) { cancelEdit(); }
