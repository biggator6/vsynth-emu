// A V-Synth wave read from its file, and a project's wave found by its number (PluginPatchFiles.h).  Moved from
// PluginProcessor.cpp as they stood (v195).  And a patch read from its file, its folder kept for the next (the
// processor's loadPatch and lastPatchFolder, moved as they stood in v198); applyPatch is the processor's.
#include "PluginPatchFiles.h"
#include "PluginAppFolder.h"
#include "PluginProcessor.h"

bool readVSynthWave(const juce::File& file, XtWaveFile& wave, std::string* why) {
    juce::MemoryBlock bytes;
    return file.loadFileAsData(bytes) && xtWaveRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), wave, why);
}

// A project keeps its waves in Wave beside Patch, each named by its number.  A copy named "… 2.wav" sorts after its original.
juce::File findWave(const juce::File& folder, int number) {
    if (number <= 0 || !folder.isDirectory()) return {};
    juce::Array<juce::File> found;
    folder.findChildFiles(found, juce::File::findFiles, false, juce::String(number).paddedLeft('0', 3) + " *");
    juce::File best;
    for (const auto& f : found)
        if ((f.hasFileExtension("vpw") || f.hasFileExtension("wav")) && (best == juce::File() || f.getFileName().length() < best.getFileName().length()))
            best = f;
    return best;
}

// ─── A V-Synth patch ─────────────────────────────────────────────────────────

static const char* const kPatchFolderKey = "patchFolder";

bool VSynthEmuProcessor::loadPatch(const juce::File& file, int zone) {
    if (!xtSynthRomReady()) { setStatus("A patch needs the firmware image"); return false; }
    juce::MemoryBlock bytes;
    XtPatchFile patch;
    std::string why;
    if (!file.loadFileAsData(bytes) || !xtPatchRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), patch, &why)) {
        setStatus(file.getFileName() + ": " + (why.empty() ? juce::String("can't be read") : juce::String(why)));
        return false;
    }
    keepSetting(kPatchFolderKey, file.getParentDirectory().getFullPathName());
    applyPatch(patch.name, patch.store, zone, file.getParentDirectory().getSiblingFile("Wave"));
    return true;
}

juce::File VSynthEmuProcessor::lastPatchFolder() const {
    const juce::File f(appSettings()->getValue(kPatchFolderKey));
    return f.isDirectory() ? f : juce::File::getSpecialLocation(juce::File::userHomeDirectory);
}
