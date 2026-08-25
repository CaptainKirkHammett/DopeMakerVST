#include "PresetManagerPanel.h"
#include "SuomiLookAndFeel.h"

//==============================================================================
// Themed confirmation dialog drawn inside the preset panel.
class PresetManagerPanel::ConfirmOverlay : public juce::Component
{
public:
    ConfirmOverlay (const juce::String& titleText, const juce::String& presetName,
                    const juce::String& messageText, const juce::String& confirmText,
                    std::function<void()> onYes, std::function<void()> onNo)
        : confirmCallback (std::move (onYes)),
          cancelCallback  (std::move (onNo)),
          yesButton (confirmText),
          noButton  ("Cancel")
    {
        setInterceptsMouseClicks (true, true);

        titleLabel.setText (titleText, juce::dontSendNotification);
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcc88ff));
        addAndMakeVisible (titleLabel);

        // Preset name on its own line — truncated with ellipsis for long names.
        nameLabel.setText (presetName, juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa0d8ff));
        nameLabel.setMinimumHorizontalScale (0.0f); // allow ellipsis truncation
        addAndMakeVisible (nameLabel);

        messageLabel.setText (messageText, juce::dontSendNotification);
        messageLabel.setJustificationType (juce::Justification::centred);
        messageLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (messageLabel);

        yesButton.onClick = [this] { if (confirmCallback) confirmCallback(); };
        noButton.onClick  = [this] { if (cancelCallback)  cancelCallback(); };
        addAndMakeVisible (yesButton);
        addAndMakeVisible (noButton);
    }

    void paint (juce::Graphics& g) override
    {
        // Semi-transparent backdrop over the whole panel.
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillAll();

        // Dialog card — same style as the preset panel itself.
        auto db = getDialogBounds();
        g.setColour (juce::Colour (0xff18182a));
        g.fillRoundedRectangle (db, 6.0f);
        g.setColour (juce::Colour (0xffcc88ff).withAlpha (0.55f));
        g.drawRoundedRectangle (db.reduced (0.5f), 6.0f, 1.5f);
    }

    void resized() override
    {
        auto area = getDialogBounds().toNearestInt().reduced (14);

        titleLabel.setBounds (area.removeFromTop (22));
        area.removeFromTop (4);
        nameLabel.setBounds (area.removeFromTop (20));
        area.removeFromTop (2);
        messageLabel.setBounds (area.removeFromTop (18));
        area.removeFromTop (10);

        auto btnRow = area.removeFromTop (24);
        const int btnW  = 84;
        const int gap   = 10;
        const int total = btnW * 2 + gap;
        const int startX = btnRow.getCentreX() - total / 2;
        noButton .setBounds (startX, btnRow.getY(), btnW, 22);
        yesButton.setBounds (startX + btnW + gap, btnRow.getY(), btnW, 22);
    }

    // Consume all mouse clicks so the panel beneath stays inert.
    void mouseDown (const juce::MouseEvent&) override {}

private:
    juce::Rectangle<float> getDialogBounds() const
    {
        const float w = 310.0f;
        const float h = 140.0f;
        return { (getWidth() - w) / 2.0f, (getHeight() - h) / 2.0f, w, h };
    }

    std::function<void()> confirmCallback;
    std::function<void()> cancelCallback;
    juce::Label      titleLabel;
    juce::Label      nameLabel;
    juce::Label      messageLabel;
    juce::TextButton yesButton;
    juce::TextButton noButton;
};

//==============================================================================
PresetManagerPanel::~PresetManagerPanel() = default;

PresetManagerPanel::PresetManagerPanel (PresetManager& pm)
    : presetManager (pm)
{
    titleLabel.setText ("PRESETS", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcc88ff));
    addAndMakeVisible (titleLabel);

    presetList.setModel (this);
    presetList.setRowHeight (20);
    presetList.setMultipleSelectionEnabled (false);
    presetList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1a1a2e));
    presetList.setColour (juce::ListBox::outlineColourId,    juce::Colour (0xff444466));
    addAndMakeVisible (presetList);

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
    hideConfirm();
    setVisible (false);
}

void PresetManagerPanel::refreshPresetList()
{
    presetNames = presetManager.getPresetNames();
    presetList.updateContent();

    const auto& current = presetManager.getCurrentPresetName();
    if (current.isNotEmpty())
    {
        const int idx = presetNames.indexOf (current);
        if (idx >= 0)
        {
            presetList.selectRow (idx, juce::dontSendNotification);
            presetList.scrollToEnsureRowIsOnscreen (idx);
        }
    }
}

//==============================================================================
// ListBoxModel
//==============================================================================
int PresetManagerPanel::getNumRows()
{
    return presetNames.size();
}

void PresetManagerPanel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                            int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= presetNames.size())
        return;

    if (rowIsSelected)
    {
        g.setColour (juce::Colour (0xff7b3fbf).withAlpha (0.65f));
        g.fillRect (0, 0, width, height);
    }

    if (auto* slf = dynamic_cast<SuomiLookAndFeel*> (&presetList.getLookAndFeel()))
        g.setFont (slf->getLabelFontRegular (15.0f));

    g.setColour (rowIsSelected ? juce::Colours::white
                               : juce::Colours::white.withAlpha (0.85f));
    g.drawText (presetNames[rowNumber], 8, 0, width - 16, height,
                juce::Justification::centredLeft, true);
}

void PresetManagerPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < presetNames.size())
        onLoad (presetNames[row]);
}

//==============================================================================
void PresetManagerPanel::navigateBy (int delta)
{
    const auto names = presetManager.getPresetNames();
    const int n = names.size();
    if (n == 0)
        return;

    const auto& current = presetManager.getCurrentPresetName();
    const int cur  = names.indexOf (current);
    const int next = (cur < 0) ? 0 : ((cur + delta + n) % n);

    onLoad (names[next]);

    // Keep internal state in sync even if the panel isn't visible.
    presetNames = names;
    presetList.updateContent();
    presetList.selectRow (next, juce::dontSendNotification);
    presetList.scrollToEnsureRowIsOnscreen (next);
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

    // Prompt before overwriting an existing preset.
    if (presetNames.contains (name))
    {
        showConfirm ("Overwrite Preset", name,
                     "Already exists. Overwrite?",
                     "Overwrite",
                     [this, name]
                     {
                         hideConfirm();
                         presetManager.savePreset (name, currentSampleFile);
                         refreshPresetList();
                         const int idx = presetNames.indexOf (name);
                         if (idx >= 0)
                             presetList.selectRow (idx, juce::dontSendNotification);
                     });
        return;
    }

    presetManager.savePreset (name, currentSampleFile);
    refreshPresetList();
    const int idx = presetNames.indexOf (name);
    if (idx >= 0)
        presetList.selectRow (idx, juce::dontSendNotification);
}

void PresetManagerPanel::onDelete()
{
    const int selectedRow = presetList.getSelectedRow();
    if (selectedRow < 0 || selectedRow >= presetNames.size())
        return;

    const auto name = presetNames[selectedRow];

    showConfirm ("Delete Preset", name,
                 "This cannot be undone.",
                 "Delete",
                 [this, name]
                 {
                     hideConfirm();
                     presetManager.deletePreset (name);
                     refreshPresetList();
                     nameEditor.setText ("New Preset", juce::dontSendNotification);
                 });
}

//==============================================================================
void PresetManagerPanel::showConfirm (const juce::String& title, const juce::String& presetName,
                                       const juce::String& message, const juce::String& confirmText,
                                       std::function<void()> onConfirm)
{
    hideConfirm();
    confirmOverlay = std::make_unique<ConfirmOverlay> (
        title, presetName, message, confirmText,
        std::move (onConfirm),
        [this] { hideConfirm(); });
    addAndMakeVisible (*confirmOverlay);
    confirmOverlay->setBounds (getLocalBounds());
    confirmOverlay->toFront (true);
}

void PresetManagerPanel::hideConfirm()
{
    if (confirmOverlay)
    {
        removeChildComponent (confirmOverlay.get());
        // Defer destruction — hideConfirm() may be called from inside the
        // overlay's button callback.  Destroying the overlay synchronously
        // would free the std::function (and its captured variables) while
        // still executing inside it, plus destroy the JUCE Button whose
        // click handler is on the call stack.  Moving ownership into an
        // async lambda keeps the overlay alive until the current message
        // handler finishes.
        auto prevent = std::shared_ptr<ConfirmOverlay> (confirmOverlay.release());
        juce::MessageManager::callAsync ([prevent] {});
    }
}

//==============================================================================
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

    // Scrollable preset list (max 15 rows visible).
    {
        const int listH = juce::jmin (presetList.getRowHeight() * 15,
                                       area.getHeight() - 34);
        presetList.setBounds (area.removeFromTop (listH));
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

    if (confirmOverlay)
        confirmOverlay->setBounds (getLocalBounds());
}
