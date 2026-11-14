// The firmware images the processor plays from: found, opened and kept (moved from PluginProcessor.cpp as it stood,
// v198, with the VC-2's rom.4 beside the V-Synth's rom.2).  The images themselves, once a process, are XtSynthRom.cpp's.
#include "PluginProcessor.h"
#include "PluginAppFolder.h"

#include <cstdlib>

// ─── The firmware images ──────────────────────────────────────────────────────

// As research/ghidra_scripts/unpack_firmware.py writes them, both in the one folder
static const char* const kRomName = "V-SynthXT.rom.2.bin";
static const char* const kVc2RomName = "V-SynthXT.rom.4.bin";
static const char* const kRomKey  = "romPath";
static const char* const kVc2RomKey = "vc2RomPath";

// Where a new instance looks for an image, after what it names first: beside the plugin, never in its bundle, where an
// added file breaks the signature; the plugin's own folder; the image last chosen by hand, which is how an AU, a VST3 and
// a standalone, each in a folder of its own, share one.
static void addPlaces(juce::StringArray& cand, const char* name, const char* key) {
    cand.add(juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile(name).getFullPathName());
    cand.add(appDir().getChildFile(name).getFullPathName());
    cand.add(appSettings()->getValue(key));
}

// A new instance's search for rom.2, first to last: VSE_XT_ROM, then the places.  rom.4 is sought at every start, with
// rom.2 or without it (openRom, or here), so both images are in before any switch of modes (the user's decision,
// 2026-11-14)
void VSynthEmuProcessor::findRom() {
    juce::StringArray cand;
    if (const char* p = std::getenv("VSE_XT_ROM")) cand.add(juce::String::fromUTF8(p));
    addPlaces(cand, kRomName, kRomKey);
    for (const auto& c : cand)
        if (c.isNotEmpty() && juce::File(c).existsAsFile() && openRom(juce::File(c))) return;
    setStatus("No ROM: choose V-SynthXT.rom.2.bin, or put it next to the plugin");
    findVc2Rom(juce::File());
}

bool VSynthEmuProcessor::openRom(const juce::File& file) {
    if (std::string why; !file.existsAsFile() || !xtSynthSetRom(file.getFullPathName().toStdString(), &why)) {
        setStatus("Not a usable rom.2 image: " + file.getFileName() + (why.empty() ? "" : ": ") + juce::String(why));
        return false;
    }
    apvts.state.setProperty(kRomKey, file.getFullPathName(), nullptr);
    { const std::lock_guard<std::mutex> lock(fxLock_); romFile_ = file.getFullPathName(); }
    setStatus("ROM ready");
    reencode(0);
    reencode(1);
    findVc2Rom(file);
    return true;
}

bool VSynthEmuProcessor::setRomFile(const juce::File& file) {
    if (!openRom(file)) return false;
    keepSetting(kRomKey, file.getFullPathName());
    return true;
}

// rom.4: beside the rom.2 just opened first, if there is one, where the unpacker leaves both, then the places.  Quiet:
// a file there that isn't the VC-2's image is passed over, and the V-Synth plays as it did without one.
void VSynthEmuProcessor::findVc2Rom(const juce::File& rom) {
    if (hasVc2Rom()) return;
    juce::StringArray cand;
    if (rom != juce::File())
        cand.add(rom.getSiblingFile(kVc2RomName).getFullPathName());
    addPlaces(cand, kVc2RomName, kVc2RomKey);
    for (const auto& c : cand)
        if (c.isNotEmpty() && juce::File(c).existsAsFile() && xtSynthSetVc2Rom(c.toStdString())) return;
}

bool VSynthEmuProcessor::setVc2RomFile(const juce::File& file) {
    if (std::string why; !file.existsAsFile() || !xtSynthSetVc2Rom(file.getFullPathName().toStdString(), &why)) {
        setStatus("Not a usable rom.4 image: " + file.getFileName() + (why.empty() ? "" : ": ") + juce::String(why));
        return false;
    }
    keepSetting(kVc2RomKey, file.getFullPathName());
    return true;
}
