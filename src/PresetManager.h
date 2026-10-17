#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Manages presets for Dope Maker VST, organised into banks.
// Each bank is a subfolder under the Presets root, containing .dmpreset
// files and a Samples/ subfolder for bundled audio.
//
// Layout on disk:
//   <AppData>/DopeMakerVST/Presets/
//       Fvcktory Bank/
//           MyPreset.dmpreset
//           Samples/
//               MyPreset_drums.wav
//       User Bank/
//           ...
class PresetManager
{
public:
    static const juce::String kDefaultBank;
    static const juce::String kPresetExtension;
    static const juce::String kSamplesFolder;

    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);

    // Root directory: <AppData>/DopeMakerVST/Presets/
    static juce::File getPresetsRoot();

    //==================================================================
    // Bank management
    //==================================================================

    // Alphabetically sorted list of all bank names on disk.
    juce::StringArray getBankNames() const;

    const juce::String& getCurrentBankName() const noexcept { return currentBankName; }

    // Switch to an existing bank. Clears the current preset name.
    void setCurrentBank (const juce::String& bankName);

    // Create a new empty bank folder. Returns true on success.
    bool createBank (const juce::String& bankName);

    // Delete a bank and all its presets. Returns true on success.
    bool deleteBank (const juce::String& bankName);

    //==================================================================
    // Bank directory for the active bank.
    juce::File getBankDirectory() const;

    // Samples subfolder inside the active bank.
    juce::File getSamplesDirectory() const;

    // Alphabetically sorted list of preset names (without extension).
    juce::StringArray getPresetNames() const;

    const juce::String& getCurrentPresetName() const noexcept { return currentPresetName; }
    void setCurrentPresetName (const juce::String& name)       { currentPresetName = name; }
    void setCurrentBankName   (const juce::String& name)       { currentBankName = name; }

    // Saves the current APVTS state and optionally copies the sample file.
    // Returns true on success.
    bool savePreset (const juce::String& presetName, const juce::File& sampleFile);

    // Restores APVTS state from disk; returns the associated sample File
    // (may be invalid if none was saved with the preset).
    juce::File loadPreset (const juce::String& presetName);

    void deletePreset (const juce::String& presetName);

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentBankName;
    juce::String currentPresetName;

    juce::File getPresetFile (const juce::String& presetName) const;
    static juce::String sanitizeFileName (const juce::String& name);

   #if JUCE_MAC
    void copySystemPresetsIfNeeded();
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
