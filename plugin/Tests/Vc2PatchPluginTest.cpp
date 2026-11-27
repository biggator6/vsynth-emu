// A VC-2 patch's load and its session (v452; PluginVc2Patch.cpp), VSynthEmuArpPluginTest's part for them.  Each of the
// disk image's 128 .p02, written to a file and loaded: every VC-2 parameter but the system's as the patch's store reads it
// in its zone and COSM (Vc2PatchStore, held to rom.4's own reads by xt_vc2patch), the store kept whole, the name its
// own, and no V-Synth parameter moved.  Files that aren't one refused, nothing moved.  A session after a load: the VC-2's
// parameters, the store and the name back in a new instance; one from before v452 (no store) its parameters as saved and
// no store, in a new instance and in one that had a patch; one from before v451 too.
#include "Vc2PatchPluginTest.h"
#include "TestChecks.h"
#include "TestControls.h"
#include "TestPatches.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginProcessor.h"
#include "../Source/PluginVc2Params.h"
#include "../Source/Vc2PatchFile.h"
#include "../Source/Vc2PatchStore.h"
#include <optional>

namespace {

constexpr const char* kRom4 = "V-SynthXT.rom.4.bin";
constexpr int kSystemLast = 0x11f, kTypedFirst = 0x56a;   // [FW] rom.4's system IDs end there; the typed from there
constexpr int kStartOffsets[] = { 0x306, 0x33a };         // [FW] 32 bits, on the V-Synth's range (startOffsetParam)
constexpr size_t kDiskPatches = 128;
// The disk image's .p02s: TestPatches.h's (v455, shared with VSynthEmuVc2PlayTest)

// What a store says of a VC-2 parameter, its ID in its zone and COSM; nothing for the system's, which no patch holds
std::optional<int> storeSays(const std::vector<uint8_t>& store, const Vc2HostParam& h) {
    if (h.id <= kSystemLast) return std::nullopt;
    if (h.id >= kTypedFirst) return vc2StoreTyped(store, h.id, h.zone, h.cosm);
    const int v = xtStoreValue(vc2Store(), store, h.id, h.zone, h.cosm);
    return std::ranges::find(kStartOffsets, h.id) != std::end(kStartOffsets) ? startOffsetParam(v) : v;
}

// Every VC-2 parameter of `p` as `store` says; how many differ
long againstStore(VSynthEmuProcessor& p, const std::vector<uint8_t>& store) {
    long wrong = 0;
    for (const Vc2HostParam& h : vc2HostParams())
        if (const auto want = storeSays(store, h)) {
            auto* r = p.apvts.getParameter(h.paramId);
            if (!r || r->convertFrom0to1(r->getValue()) != (float)*want) ++wrong;
        }
    return wrong;
}

// The VC-2's system parameters, which no patch holds, by ID: with `top`, each moved to its top first
std::map<juce::String, float> systemOf(VSynthEmuProcessor& p, bool top) {
    std::map<juce::String, float> out;
    for (const Vc2HostParam& h : vc2HostParams())
        if (auto* r = p.apvts.getParameter(h.paramId); r && h.id <= kSystemLast) {
            if (top) r->setValueNotifyingHost(1.0f);
            out[h.paramId] = r->getValue();
        }
    return out;
}

void eachFile(TestChecks& check, const std::vector<DiskPatch>& patches, const juce::File& work) {
    VSynthEmuProcessor p;
    const auto system = systemOf(p, true);
    long wrong = 0, refused = 0, movedOthers = 0;
    for (const DiskPatch& d : patches) {
        const juce::File file = work.getChildFile("vc2").getChildFile(d.name);
        writeTestFile(file, d.bytes);
        Vc2PatchFile read;
        const auto before = parameterValues(p, false);
        if (!vc2PatchRead(d.bytes.data(), d.bytes.size(), read) || !p.loadVc2Patch(file)) {
            ++refused;
            continue;
        }
        const bool asRead = againstStore(p, read.store) == 0 && p.vc2PatchStore() == read.store
                            && p.vc2PatchName() == juce::String(read.name);
        if (!asRead && wrong++ < 3) check(false, d.name + " isn't on the VC-2's parameters as its store reads");
        if (parameterValues(p, false) != before || systemOf(p, false) != system) ++movedOthers;
    }
    check(patches.size() == kDiskPatches, "the disk image's 128 VC-2 patches, not " + std::to_string(patches.size()));
    check(refused == 0 && wrong == 0, std::to_string(refused) + " refused, " + std::to_string(wrong) + " not as read");
    check(movedOthers == 0, std::to_string(movedOthers) + " moved a V-Synth parameter or a VC-2 system one");
}

// A V-Synth patch, a file of text, one too short and none: refused, nothing moved
void notOnes(TestChecks& check, const juce::File& rom, const juce::File& work) {
    VSynthEmuProcessor p;
    const std::vector<ImagePatch> vsynth = imagePatches(rom);
    const juce::File p00 = work.getChildFile("not/a.p00"), text = work.getChildFile("not/text.p02");
    const juce::File little = work.getChildFile("not/little.p02");
    writeTestFile(p00, vsynth.empty() ? std::vector<uint8_t>() : vsynth.front().file);
    check(text.replaceWithText("not a patch at all, though long enough for its headers"), "the text file written");
    writeTestFile(little, { 0, 0x1e, 'V', 'C' });
    const auto before = parameterValues(p, true);
    for (const juce::File& f : { p00, text, little, work.getChildFile("not/none.p02") })
        check(!p.loadVc2Patch(f), f.getFileName().toStdString() + " should be refused");
    check(parameterValues(p, true) == before && p.vc2PatchStore().empty(), "and nothing moved, no store kept");
}

// A session after a load, and ones from before
void sessions(TestChecks& check, const std::vector<DiskPatch>& patches, const juce::File& work) {
    if (patches.size() < 3) return;
    const juce::File file = work.getChildFile("vc2s").getChildFile(patches[1].name);
    writeTestFile(file, patches[1].bytes);
    VSynthEmuProcessor p;
    check(p.loadVc2Patch(file), "a patch should load for the session");
    juce::MemoryBlock state;
    p.getStateInformation(state);
    VSynthEmuProcessor back;
    back.setStateInformation(state.getData(), (int)state.getSize());
    check(parameterValues(back, true) == parameterValues(p, true) && back.vc2PatchStore() == p.vc2PatchStore()
              && back.vc2PatchName() == p.vc2PatchName(),
          "a session should bring the VC-2 patch back: its parameters, store and name");
    const juce::MemoryBlock v451 = sessionBefore(sessionBefore(state, "\x01", "vc2Store"), "\x01", "vc2PatchName");
    VSynthEmuProcessor old, had;
    old.setStateInformation(v451.getData(), (int)v451.getSize());
    check(parameterValues(old, true) == parameterValues(p, true) && old.vc2PatchStore().empty(),
          "a session from before v452: its parameters as saved, no store");
    const juce::File other = work.getChildFile("vc2s").getChildFile(patches[2].name);
    writeTestFile(other, patches[2].bytes);
    check(had.loadVc2Patch(other), "another patch should load");
    had.setStateInformation(v451.getData(), (int)v451.getSize());
    check(had.vc2PatchStore().empty() && had.vc2PatchName().isEmpty(), "and into an instance with a patch: none kept");
    const juce::MemoryBlock v450 = sessionBefore(v451, kVc2ParamPrefix, nullptr);
    VSynthEmuProcessor older, fresh;
    older.setStateInformation(v450.getData(), (int)v450.getSize());
    check(parameterValues(older, true) == parameterValues(fresh, true) && older.vc2PatchStore().empty(),
          "a session from before v451: the VC-2's at their defaults, no store");
}

} // namespace

int vc2PatchPluginTest(const juce::File& rom, const juce::File& work) {
    TestChecks check { "vc2patch" };
    const std::string rom4 = rom.getSiblingFile(kRom4).getFullPathName().toStdString();
    check(xtSynthSetVc2Rom(rom4), "rom.4 should load beside rom.2");
    const std::vector<DiskPatch> patches = diskVc2Patches(rom);
    eachFile(check, patches, work);
    notOnes(check, rom, work);
    sessions(check, patches, work);
    return check.summary();
}
