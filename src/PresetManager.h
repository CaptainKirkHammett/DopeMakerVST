#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Manages presets for Dope Maker VST.
// Each preset is an XML file storing the full APVTS state and an optional
// relative path to its audio sample (which is copied into a Samples/ subfolder
// inside the bank directory when saving).
//
// Layout on disk:
//   <AppData>/DopeMakerVST/Presets/Default Bank/
//       MyPreset.dmpreset
//       Samples/
//           MyPreset_drums.wav
class PresetManager
{
public:
    static const juce::String kDefaultBank;
    static const juce::String kPresetExtension;
    static const juce::String kSamplesFolder;

    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);

    // Root directory: <AppData>/DopeMakerVST/Presets/
    static juce::File getPresetsRoot();

    // Bank directory (Default Bank).
    juce::File getBankDirectory() const;

    // Samples subfolder inside the bank.
    juce::File getSamplesDirectory() const;

    // Alphabetically sorted list of preset names (without extension).
    juce::StringArray getPresetNames() const;

    const juce::String& getCurrentPresetName() const noexcept { return currentPresetName; }

    // Saves the current APVTS state and optionally copies the sample file.
    // Returns true on success.
    bool savePreset (const juce::String& presetName, const juce::File& sampleFile);

    // Restores APVTS state from disk; returns the associated sample File
    // (may be invalid if none was saved with the preset).
    juce::File loadPreset (const juce::String& presetName);

    void deletePreset (const juce::String& presetName);

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentPresetName;

    juce::File getPresetFile (const juce::String& presetName) const;
    static juce::String sanitizeFileName (const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
