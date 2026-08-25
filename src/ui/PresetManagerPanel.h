#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PresetManager.h"

// Floating overlay panel for preset browsing and saving.
// Add it as a child of uiRoot with setVisible(false) and toggle visibility
// via show() / hide().  Position it before making visible.
class PresetManagerPanel : public juce::Component,
                           private juce::ListBoxModel
{
public:
    // Called after a preset is loaded; argument is the bundled sample file
    // (may be invalid if the preset had no sample).
    std::function<void (const juce::File&)> onPresetLoaded;

    explicit PresetManagerPanel (PresetManager& pm);
    ~PresetManagerPanel() override;

    void show (juce::Rectangle<int> anchorBounds);
    void hide();

    // Editor must call this whenever the loaded sample changes so the panel
    // can bundle it when saving.
    void setCurrentSampleFile (const juce::File& f) { currentSampleFile = f; }

    void refreshPresetList();

    // Navigate to the next (+1) or previous (-1) preset.
    void navigateBy (int delta);

    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    PresetManager& presetManager;
    juce::File     currentSampleFile;

    juce::Label       titleLabel;
    juce::ListBox     presetList;
    juce::StringArray presetNames;
    juce::TextEditor  nameEditor;
    juce::TextButton  saveButton   { "Save" };
    juce::TextButton  deleteButton { "Delete" };
    juce::TextButton  closeButton  { "X" };

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g,
                           int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void onLoad (const juce::String& name);
    void onSave();
    void onDelete();

    // Themed confirmation overlay (replaces system AlertWindow).
    class ConfirmOverlay;
    std::unique_ptr<ConfirmOverlay> confirmOverlay;
    void showConfirm (const juce::String& title, const juce::String& presetName,
                      const juce::String& message, const juce::String& confirmText,
                      std::function<void()> onConfirm);
    void hideConfirm();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManagerPanel)
};
