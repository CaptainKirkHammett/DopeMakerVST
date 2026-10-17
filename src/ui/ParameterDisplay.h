#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// LCD display that shows the name and formatted value of the last interacted
// parameter.  Clicking the display opens an inline text editor so the user can
// type a precise numeric value; pressing Enter commits it, Escape cancels.
class ParameterDisplay : public juce::Component,
                         private juce::TextEditor::Listener
{
public:
    explicit ParameterDisplay (juce::AudioProcessorValueTreeState& apvts);

    void paint   (juce::Graphics&) override;
    void resized () override;

    // Updates the displayed name, value text, and the parameter ID used when
    // the user types a new value.  Safe to call on any thread.
    void showParameter (const juce::String& name,
                        const juce::String& value,
                        const juce::String& paramId);

private:
    juce::AudioProcessorValueTreeState& apvts;

    juce::String paramName  { "---" };
    juce::String paramValue { "---" };
    juce::String currentParamId;

    juce::TextEditor editor;
    bool isEditing { false };

    void mouseUp (const juce::MouseEvent&) override;

    void enterEditMode();
    void commitEdit();
    void cancelEdit();

    // TextEditor::Listener — Enter commits, Escape / focus loss cancels.
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;
    void textEditorFocusLost        (juce::TextEditor&) override;

    // Applies the same decimal trimming used in paint() so that the editor
    // pre-fill and the painted display always show the same number of decimals.
    juce::String formattedDisplayValue() const;

    // Returns the lcd rect (inset from bounds) that layout is based on.
    juce::Rectangle<float> lcdRect() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterDisplay)
};
