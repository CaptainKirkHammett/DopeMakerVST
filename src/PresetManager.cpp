#include "PresetManager.h"

const juce::String PresetManager::kDefaultBank    = "Fvcktory Bank";
const juce::String PresetManager::kPresetExtension = ".dmpreset";
const juce::String PresetManager::kSamplesFolder   = "Samples";

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& a)
    : apvts (a),
      currentBankName (kDefaultBank)
{
    // Migrate legacy "Default Bank" folder to the new name.
    auto legacyDir = getPresetsRoot().getChildFile ("Default Bank");
    auto newDir    = getPresetsRoot().getChildFile (kDefaultBank);
    if (legacyDir.isDirectory() && ! newDir.exists())
        legacyDir.moveFileTo (newDir);

    getBankDirectory().createDirectory();
    getSamplesDirectory().createDirectory();

    // macOS: copy factory presets from system-wide install location to user location.
    // The .pkg installer writes to /Library/Application Support/ (no user detection needed).
    // On first load we copy anything missing into ~/Library/Application Support/.
   #if JUCE_MAC
    copySystemPresetsIfNeeded();
   #endif
}

juce::File PresetManager::getPresetsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DopeMakerVST")
               .getChildFile ("Presets");
}

//==============================================================================
// Bank management
//==============================================================================

juce::StringArray PresetManager::getBankNames() const
{
    juce::StringArray names;
    for (auto& d : getPresetsRoot().findChildFiles (
             juce::File::findDirectories, false))
    {
        // Skip the Samples subfolder if it somehow sits at root level.
        if (d.getFileName().equalsIgnoreCase (kSamplesFolder))
            continue;
        names.add (d.getFileName());
    }
    names.sort (true);
    return names;
}

void PresetManager::setCurrentBank (const juce::String& bankName)
{
    currentBankName   = bankName;
    currentPresetName = {};
    getBankDirectory().createDirectory();
    getSamplesDirectory().createDirectory();
}

bool PresetManager::createBank (const juce::String& bankName)
{
    const auto safe = sanitizeFileName (bankName);
    if (safe.isEmpty())
        return false;

    auto dir = getPresetsRoot().getChildFile (safe);
    if (dir.isDirectory())
        return false; // already exists

    if (! dir.createDirectory())
        return false;

    dir.getChildFile (kSamplesFolder).createDirectory();
    return true;
}

bool PresetManager::deleteBank (const juce::String& bankName)
{
    // Never delete the factory bank.
    if (bankName == kDefaultBank)
        return false;

    auto dir = getPresetsRoot().getChildFile (bankName);
    if (! dir.isDirectory())
        return false;

    const bool ok = dir.deleteRecursively();
    if (ok && currentBankName == bankName)
        setCurrentBank (kDefaultBank);
    return ok;
}

//==============================================================================

juce::File PresetManager::getBankDirectory() const
{
    return getPresetsRoot().getChildFile (currentBankName);
}

juce::File PresetManager::getSamplesDirectory() const
{
    return getBankDirectory().getChildFile (kSamplesFolder);
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (auto& f : getBankDirectory().findChildFiles (
             juce::File::findFiles, false, "*" + kPresetExtension))
    {
        juce::String displayName = f.getFileNameWithoutExtension();
        if (auto xml = juce::XmlDocument::parse (f))
            if (xml->hasAttribute ("name"))
                displayName = xml->getStringAttribute ("name");
        names.add (displayName);
    }
    names.sort (true);
    return names;
}

bool PresetManager::savePreset (const juce::String& presetName, const juce::File& sampleFile)
{
    const auto safe = sanitizeFileName (presetName);
    if (safe.isEmpty())
        return false;

    // Copy sample into the bank's Samples/ subfolder if one is provided.
    juce::String relSamplePath;
    if (sampleFile.existsAsFile())
    {
        const auto dest = getSamplesDirectory()
                              .getChildFile (sampleFile.getFileName());
        if (dest != sampleFile)
            sampleFile.copyFileTo (dest);
        relSamplePath = kSamplesFolder + "/" + dest.getFileName();
    }

    // Serialize APVTS state to XML.
    auto stateTree = apvts.copyState();
    auto stateXml  = stateTree.createXml();

    juce::XmlElement root ("DopeMakerPreset");
    root.setAttribute ("version", 1);
    root.setAttribute ("name", presetName);

    if (relSamplePath.isNotEmpty())
    {
        auto* sampleEl = root.createNewChildElement ("SampleFile");
        sampleEl->setAttribute ("relPath", relSamplePath);
    }

    auto* paramsEl = root.createNewChildElement ("Parameters");
    paramsEl->addChildElement (stateXml.release());

    const bool ok = root.writeTo (getPresetFile (safe));
    if (ok)
        currentPresetName = presetName;
    return ok;
}

juce::File PresetManager::loadPreset (const juce::String& presetName)
{
    const auto file = getPresetFile (sanitizeFileName (presetName));
    if (! file.existsAsFile())
        return {};

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || xml->getTagName() != "DopeMakerPreset")
        return {};

    // Restore APVTS state.
    if (auto* paramsEl = xml->getChildByName ("Parameters"))
    {
        if (auto* stateXml = paramsEl->getFirstChildElement())
        {
            auto tree = juce::ValueTree::fromXml (*stateXml);
            if (tree.isValid())
                apvts.replaceState (tree);
        }
    }

    currentPresetName = presetName;

    // Return the bundled sample file (may be invalid if none was saved).
    if (auto* sampleEl = xml->getChildByName ("SampleFile"))
    {
        const auto rel = sampleEl->getStringAttribute ("relPath");
        if (rel.isNotEmpty())
            return getBankDirectory().getChildFile (rel);
    }
    return {};
}

void PresetManager::deletePreset (const juce::String& presetName)
{
    getPresetFile (sanitizeFileName (presetName)).deleteFile();
    if (currentPresetName == presetName)
        currentPresetName = {};
}

juce::File PresetManager::getPresetFile (const juce::String& presetName) const
{
    return getBankDirectory().getChildFile (presetName + kPresetExtension);
}

juce::String PresetManager::sanitizeFileName (const juce::String& name)
{
    juce::String safe;
    for (auto ch : name)
    {
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' ||
            ch == '?' || ch == '"'  || ch == '<' || ch == '>' || ch == '|')
            safe += '_';
        else
            safe += ch;
    }
    return safe.trim();
}

#if JUCE_MAC
void PresetManager::copySystemPresetsIfNeeded()
{
    // System-wide location where the .pkg installer writes factory presets.
    auto systemBank = juce::File ("/Library/Application Support/DopeMakerVST/Presets")
                          .getChildFile (kDefaultBank);

    if (! systemBank.isDirectory())
        return;

    auto userBank    = getPresetsRoot().getChildFile (kDefaultBank);
    auto userSamples = userBank.getChildFile (kSamplesFolder);
    userBank.createDirectory();
    userSamples.createDirectory();

    // Copy presets (don't overwrite existing).
    for (auto& f : systemBank.findChildFiles (juce::File::findFiles, false, "*.dmpreset"))
    {
        auto dest = userBank.getChildFile (f.getFileName());
        if (! dest.exists())
            f.copyFileTo (dest);
    }

    // Copy samples (don't overwrite existing).
    auto systemSamples = systemBank.getChildFile (kSamplesFolder);
    if (systemSamples.isDirectory())
    {
        for (auto& f : systemSamples.findChildFiles (juce::File::findFiles, false, "*.wav"))
        {
            auto dest = userSamples.getChildFile (f.getFileName());
            if (! dest.exists())
                f.copyFileTo (dest);
        }
    }
}
#endif
