#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PresetManager.h"

// Floating overlay panel for preset browsing and saving.
// Add it as a child of uiRoot with setVisible(false) and toggle visibility
// via show() / hide().  Position it before making visible.
class PresetManagerPanel : public juce::Component
{
public:
    // Called after a preset is loaded; argument is the bundled sample file
    // (may be invalid if the preset had no sample).
    std::function<void (const juce::File&)> onPresetLoaded;

    explicit PresetManagerPanel (PresetManager& pm);

    void show (juce::Rectangle<int> anchorBounds);
    void hide();

    // Editor must call this whenever the loaded sample changes so the panel
    // can bundle it when saving.
    void setCurrentSampleFile (const juce::File& f) { currentSampleFile = f; }

    void refreshPresetList();

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    PresetManager& presetManager;
    juce::File     currentSampleFile;

    juce::Label      titleLabel;
    juce::ComboBox   presetBox;
    juce::TextButton prevButton  { "<" };
    juce::TextButton nextButton  { ">" };
    juce::TextEditor nameEditor;
    juce::TextButton saveButton  { "Save" };
    juce::TextButton deleteButton{ "Delete" };
    juce::TextButton closeButton { "X" };

    void onLoad (const juce::String& name);
    void onSave();
    void onDelete();
    void navigateBy (int delta);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManagerPanel)
};
