#include "PresetManagerPanel.h"

PresetManagerPanel::PresetManagerPanel (PresetManager& pm)
    : presetManager (pm)
{
    titleLabel.setText ("PRESETS", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcc88ff));
    addAndMakeVisible (titleLabel);

    presetBox.setTextWhenNothingSelected ("(no presets)");
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty())
            onLoad (name);
    };
    addAndMakeVisible (presetBox);

    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    prevButton.onClick = [this] { navigateBy (-1); };
    nextButton.onClick = [this] { navigateBy ( 1); };

    nameEditor.setMultiLine (false);
    nameEditor.setReturnKeyStartsNewLine (false);
    nameEditor.setText ("New Preset");
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1a1a2e));
    nameEditor.setColour (juce::TextEditor::textColourId,       juce::Colour (0xffa0d8ff));
    nameEditor.setColour (juce::TextEditor::outlineColourId,    juce::Colour (0xff444466));
    addAndMakeVisible (nameEditor);

    saveButton  .onClick = [this] { onSave(); };
    deleteButton.onClick = [this] { onDelete(); };
    closeButton .onClick = [this] { hide(); };

    addAndMakeVisible (saveButton);
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (closeButton);

    refreshPresetList();
}

void PresetManagerPanel::show (juce::Rectangle<int> anchorBounds)
{
    setBounds (anchorBounds);
    setVisible (true);
    toFront (false);
    refreshPresetList();
}

void PresetManagerPanel::hide()
{
    setVisible (false);
}

void PresetManagerPanel::refreshPresetList()
{
    const auto names = presetManager.getPresetNames();
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < names.size(); ++i)
        presetBox.addItem (names[i], i + 1);

    const auto& current = presetManager.getCurrentPresetName();
    if (current.isNotEmpty())
    {
        const int idx = names.indexOf (current);
        if (idx >= 0)
            presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
    }
}

void PresetManagerPanel::onLoad (const juce::String& name)
{
    if (name.isEmpty())
        return;
    const juce::File sampleFile = presetManager.loadPreset (name);
    if (onPresetLoaded)
        onPresetLoaded (sampleFile);
    nameEditor.setText (name, juce::dontSendNotification);
}

void PresetManagerPanel::onSave()
{
    const auto name = nameEditor.getText().trim();
    if (name.isEmpty())
        return;
    presetManager.savePreset (name, currentSampleFile);
    refreshPresetList();

    // Select the just-saved preset in the box.
    const auto names = presetManager.getPresetNames();
    const int  idx   = names.indexOf (name);
    if (idx >= 0)
        presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
}

void PresetManagerPanel::onDelete()
{
    const auto name = presetBox.getText();
    if (name.isEmpty())
        return;

    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle ("Delete Preset")
                       .withMessage ("Are you sure you want to delete \"" + name + "\"?")
                       .withButton ("Delete")
                       .withButton ("Cancel")
                       .withAssociatedComponent (this);

    juce::AlertWindow::showAsync (options, [this, name] (int result)
    {
        if (result != 1)
            return;
        presetManager.deletePreset (name);
        refreshPresetList();
        nameEditor.setText ("New Preset", juce::dontSendNotification);
    });
}

void PresetManagerPanel::navigateBy (int delta)
{
    const int n = presetBox.getNumItems();
    if (n == 0)
        return;
    const int cur = presetBox.getSelectedItemIndex();   // -1 if nothing selected
    const int next = (cur + delta + n) % n;
    presetBox.setSelectedId (next + 1, juce::sendNotification);
    onLoad (presetBox.getText());
}

void PresetManagerPanel::paint (juce::Graphics& g)
{
    // Dark opaque background.
    g.setColour (juce::Colour (0xff18182a));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // Pink FX bezel border.
    g.setColour (juce::Colour (0xffcc88ff).withAlpha (0.55f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.5f);
}

void PresetManagerPanel::resized()
{
    auto area = getLocalBounds().reduced (8);

    // Title row.
    {
        auto row = area.removeFromTop (24);
        closeButton.setBounds (row.removeFromRight (24).withSizeKeepingCentre (22, 20));
        titleLabel .setBounds (row);
    }

    area.removeFromTop (6);

    // Combo + prev / next row.
    {
        auto row = area.removeFromTop (26);
        prevButton.setBounds (row.removeFromLeft  (28).withSizeKeepingCentre (26, 22));
        nextButton.setBounds (row.removeFromRight (28).withSizeKeepingCentre (26, 22));
        row.removeFromLeft  (4);
        row.removeFromRight (4);
        presetBox.setBounds (row);
    }

    area.removeFromTop (8);

    // Name editor + action buttons row.
    {
        auto row = area.removeFromTop (26);
        saveButton  .setBounds (row.removeFromRight (64).withSizeKeepingCentre (62, 22));
        row.removeFromRight (4);
        deleteButton.setBounds (row.removeFromRight (64).withSizeKeepingCentre (62, 22));
        row.removeFromRight (6);
        nameEditor  .setBounds (row);
    }
}
