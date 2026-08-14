#include "PresetManager.h"

const juce::String PresetManager::kDefaultBank    = "Default Bank";
const juce::String PresetManager::kPresetExtension = ".dmpreset";
const juce::String PresetManager::kSamplesFolder   = "Samples";

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& a)
    : apvts (a)
{
    getBankDirectory().createDirectory();
    getSamplesDirectory().createDirectory();
}

juce::File PresetManager::getPresetsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DopeMakerVST")
               .getChildFile ("Presets");
}

juce::File PresetManager::getBankDirectory() const
{
    return getPresetsRoot().getChildFile (kDefaultBank);
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
        // Read the display name stored inside the file; fall back to the
        // filename for presets saved before this attribute was added.
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
                              .getChildFile (safe + "_" + sampleFile.getFileName());
        if (dest != sampleFile)
            sampleFile.copyFileTo (dest);
        relSamplePath = kSamplesFolder + "/" + dest.getFileName();
    }

    // Serialize APVTS state to XML.
    auto stateTree = apvts.copyState();
    auto stateXml  = stateTree.createXml();

    juce::XmlElement root ("DopeMakerPreset");
    root.setAttribute ("version", 1);
    root.setAttribute ("name", presetName);   // preserve original name (may contain chars sanitized in filename)

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
    // Replace characters that are illegal in file names on any OS.
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
