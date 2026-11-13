// A V-Synth wave read from its file, and a project's wave found by its number (PluginPatchFiles.h).  Moved from
// PluginProcessor.cpp as they stood (v195).
#include "PluginPatchFiles.h"

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
