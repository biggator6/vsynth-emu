#pragma once
// The disk image's patches beside the ROM, a file written for a test to load, and a session as one saved before some
// parameters came: VSynthEmuArpPluginTest's (the arpeggio's, the MSM's and the zones') and VSynthEmuRestoreTest's, made one
// when the zones' checks came (v195).
#include "TestChecks.h"
#include "../Source/PluginProcessor.h"
#include "../Source/XtFiles.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <string>
#include <vector>

// A patch of the image's: its name, its store and its file's own bytes
struct ImagePatch {
    std::string name;
    std::vector<uint8_t> store, file;
};

// Every patch in the image's Internal.prj, in its order; none without the image beside the ROM
inline std::vector<ImagePatch> imagePatches(const juce::File& rom) {
    std::vector<ImagePatch> out;
    XtDiskImage disk;
    if (!disk.open(rom.getSiblingFile("V-SynthXTDiskImage.bin.0.bin").getFullPathName().toStdString())) return out;
    for (const auto& e : disk.list("Internal.prj/Patch")) {
        XtPatchFile patch;
        const std::vector<uint8_t> bytes = disk.read(e);
        if (xtPatchRead(bytes.data(), bytes.size(), patch)) out.push_back({ patch.name, patch.store, bytes });
    }
    return out;
}

// The first-bank IDs two stores read otherwise, zone 1 and both COSMs, as "ID was→is", the first few (v602, from
// ProjectSaveTest.cpp when the project's save, v603, needed it too)
inline std::string idsMoved(const std::vector<uint8_t>& was, const std::vector<uint8_t>& is) {
    constexpr int kShown = 8;
    std::string out;
    int shown = 0;
    for (const XtPatchSectionDef& s : xtPatchSections())
        for (int id = s.first; id <= s.last && id < kXtDescIds && shown < kShown; ++id)
            for (int cosm = 0; cosm < (xtPatchIsCosm(id) ? 2 : 1); ++cosm) {
                const int a = xtPatchValue(was, id, 0, cosm), b = xtPatchValue(is, id, 0, cosm);
                if (a == b || shown >= kShown) continue;
                ++shown;
                out += juce::String::toHexString(id).toStdString() + (cosm ? "/2 " : " ") + std::to_string(a) + "→"
                       + std::to_string(b) + " ";
            }
    return out;
}

// `bytes` as `file`, its folder made
inline void writeTestFile(const juce::File& file, const std::vector<uint8_t>& bytes) {
    file.getParentDirectory().createDirectory();
    file.replaceWithData(bytes.data(), bytes.size());
}

// A processor with the ROM, `patch` loaded into it from `file` (written there first), its load checked
inline std::unique_ptr<VSynthEmuProcessor> loadedWith(TestChecks& check, const juce::File& rom, const ImagePatch& patch,
                                                      const juce::File& file) {
    writeTestFile(file, patch.file);
    auto p = std::make_unique<VSynthEmuProcessor>();
    p->setRomFile(rom);
    check(p->loadPatch(file), patch.name + " should load");
    return p;
}

// A session's state as one saved before some parameters came: those whose ID starts with `prefix` taken out, and the
// state's `property` too where one is named (the arpeggio's pattern before v190, a patch's store before v195)
inline juce::MemoryBlock sessionBefore(const juce::MemoryBlock& state, const juce::String& prefix,
                                       const char* property) {
    std::unique_ptr<juce::XmlElement> old(VSynthEmuProcessor::getXmlFromBinary(state.getData(), (int)state.getSize()));
    juce::MemoryBlock out;
    if (!old) return out;
    if (property != nullptr) old->removeAttribute(property);
    for (auto* e = old->getFirstChildElement(); e != nullptr;) {
        auto* next = e->getNextElement();
        if (e->getStringAttribute("id").startsWith(prefix)) old->removeChildElement(e, true);
        e = next;
    }
    VSynthEmuProcessor::copyXmlToBinary(*old, out);
    return out;
}
