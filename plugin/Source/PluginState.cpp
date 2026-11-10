// A session's state for the host: the parameters, the effects' edits by ID, and the paths and patch store a project needs
// back.  Moved here from PluginProcessor.cpp in v172, when what doesn't come back began to say so: a wave whose file has
// gone, and a patch's zone waves that aren't in its folder, go with what isn't here of the patch and in the status line.
#include "PluginProcessor.h"

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

    apvts.replaceState(juce::ValueTree::fromXml(*xml));
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
    restoreZones(gone);
    // What the session said already is left as it was; anything new goes with it, and in the status line
    juce::StringArray fresh;
    for (const auto& n : gone)
        if (!(", " + patchNote_ + ", ").contains(", " + n + ", ")) fresh.add(n);
    if (fresh.isEmpty()) return;
    patchNote_ << (patchNote_.isEmpty() ? "" : ", ") << fresh.joinIntoString(", ");
    setStatus("Not here: " + fresh.joinIntoString(", "));
}

// A patch's zones 2…16, from the store it kept, their waves from its folder; what isn't there goes in `notes`
void VSynthEmuProcessor::restoreZones(juce::StringArray& notes) {
    juce::MemoryOutputStream zoneStore;
    const juce::String kept = apvts.state.getProperty("patchStore").toString();
    if (kept.isNotEmpty() && xtSynthRomReady() && juce::Base64::convertFromBase64(zoneStore, kept)) {
        const auto* b = static_cast<const uint8_t*>(zoneStore.getData());
        const std::vector<uint8_t> store(b, b + zoneStore.getDataSize());
        loadZones(store, juce::File(apvts.state.getProperty("patchWaves").toString()), notes);
        return;
    }
    ++zonesGen_;
    patchZones_.store(1);
    zonesReady_.store(true);
    synth_.setZones(nullptr);
}
