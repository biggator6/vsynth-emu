// A session's state for the host: the parameters, the effects' edits by ID, and the paths and patch store a project needs
// back.  Moved here from PluginProcessor.cpp in v172, when what doesn't come back began to say so: a wave whose file has
// gone, and a patch's zone waves that aren't in its folder, go with what isn't here of the patch and in the status line.
#include "PluginProcessor.h"
#include "PluginParamIds.h"

void VSynthEmuProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    juce::String edits;
    for (int id = 0; id < kFxIds; ++id) {
        const int v = fxEdits_[id].load(std::memory_order_relaxed);
        if (v != kFxNoEdit) edits << id << "=" << v << ",";
    }
    state.setProperty("fxEdits", edits, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VSynthEmuProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (!xml || !xml->hasTagName(apvts.state.getType()))
        return;

    const juce::ValueTree saved = juce::ValueTree::fromXml(*xml);
    const bool hadSplits = saved.getChildWithProperty("id", Params::zoneUpperId(0)).isValid();   // v195 on
    apvts.replaceState(saved);
    restoreArpPattern();   // v190: a session before it has none
    for (auto& e : fxEdits_) e.store(kFxNoEdit, std::memory_order_relaxed);
    for (const auto& pair : juce::StringArray::fromTokens(apvts.state.getProperty("fxEdits").toString(), ",", ""))
        if (pair.contains("=")) setFxParam(pair.upToFirstOccurrenceOf("=", false, false).getIntValue(),
                                           pair.fromFirstOccurrenceOf("=", false, false).getIntValue());
    fxPatchPending_ = true;

    // The firmware image and the file, if their paths were persisted
    const juce::String rom = apvts.state.getProperty("romPath").toString();
    if (rom.isNotEmpty() && !xtSynthRomReady() && juce::File(rom).existsAsFile())
        openRom(juce::File(rom));
    juce::StringArray gone;   // what the session names and isn't here now
    for (int k = 0; k < 2; ++k) {
        const juce::String path = apvts.state.getProperty(k ? "loadedFilePath2" : "loadedFilePath").toString();
        if (path.isEmpty()) continue;
        const juce::File f(path);
        if (f.existsAsFile())
            loadFile(f, k, false);
        else
            gone.add(juce::String(k ? "OSC 2" : "OSC 1") + "'s wave " + f.getFileName() + ", not in " +
                     f.getParentDirectory().getFullPathName());
    }
    patchName_ = apvts.state.getProperty("patchName").toString();
    patchNote_ = apvts.state.getProperty("patchNote").toString();
    restoreZones(gone, hadSplits);   // PluginZones.cpp
    // What the session said already is left as it was; anything new goes with it, and in the status line
    juce::StringArray fresh;
    for (const auto& n : gone)
        if (!(", " + patchNote_ + ", ").contains(", " + n + ", ")) fresh.add(n);
    if (fresh.isEmpty()) return;
    patchNote_ << (patchNote_.isEmpty() ? "" : ", ") << fresh.joinIntoString(", ");
    setStatus("Not here: " + fresh.joinIntoString(", "));
}
