// A V-Synth patch saved: see PluginPatchSave.h (v602).
#include "PluginPatchSave.h"
#include "PluginPatchFiles.h"
#include "PluginProcessor.h"
#include "XtArpeggioPatch.h"
#include "XtFirmware.h"
#include "XtFwPatchFile.h"
#include "XtPatch.h"
#include "XtPatchIds.h"

using xtfile::fail;

namespace {
constexpr int kNameFirst = 0x103, kNameChars = 12;   // [FW] the patch's name, a character an ID
constexpr int kWaveNumber = 0x2be, kOscStride = 0x34;   // [FW] OSC 1's Wave Number, and OSC 2's IDs past OSC 1's
constexpr int kKeyShift = 16;   // xtPatchIds' COSM 2 (kXtCosm2Ids) and xtArpPatchIds' pattern line, above the ID
constexpr int kIdMask = (1 << kKeyShift) - 1;

// An ID of the maps as the setter takes it: the low bits the ID, the high its COSM (a COSM's IDs) or its pattern line
XtPatchSet patchSetOf(int key, int value) {
    XtPatchSet s { key & kIdMask, value };
    const int high = key >> kKeyShift;
    s.base = s.id >= kXtDescIds ? xtTypedBaseOf(s.id) : 0;
    const bool cosm = s.base ? s.base == kXtCosmSlots : xtPatchIsCosm(s.id);
    (cosm ? s.cosm : s.zone) = high;
    return s;
}

// Whether a patch holds it: the controls' maps carry the system's IDs too (the controller assigns, Output Mode, Clock
// Source, Out Gain), which xtPatchControls keeps out of a patch as well
bool inPatch(const XtPatchSet& s) {
    const XtTypedDesc* t = s.id >= kXtDescIds ? xtTypedDesc(s.id) : nullptr;
    if (s.id >= kXtDescIds) return t && s.base && xtPatchSection(s.base + t->slot, s.zone, s.cosm) >= 0;
    return xtParamDesc(s.id) && xtPatchSection(s.id, s.zone, s.cosm) >= 0;
}

int storeRead(const std::vector<uint8_t>& store, const XtPatchSet& s) {
    return s.id >= kXtDescIds ? xtPatchTyped(store, s.base, s.id, s.zone, s.cosm)
                              : xtPatchValue(store, s.id, s.zone, s.cosm);
}

// Each of `ids` of one bank the store reads otherwise, set: the first bank's (`typed` false), or the typed bank's, of
// the types the store now holds alone, as a store keeps no other type's values (their slots are the type's own)
bool setChanged(std::vector<uint8_t>& store, const XtPatchIds& ids, bool typed, std::string* why) {
    std::vector<XtPatchSet> changed;
    for (const auto& [key, value] : ids) {
        const XtPatchSet s = patchSetOf(key, value);
        if ((s.id >= kXtDescIds) != typed || !inPatch(s)) continue;
        if (typed && !xtTypedInStoredType(store, s.id, s.zone, s.cosm)) continue;
        if (storeRead(store, s) != value) changed.push_back(s);
    }
    return changed.empty() || xtFwPatchSet(store, changed, why);
}

XtPatchIds merged(XtPatchIds a, const XtPatchIds& b) {
    a.insert(b.begin(), b.end());
    return a;
}

// [FW] a COSM's switch and Type, and Type 0, Through, which the plugin plays as the COSM off (xtPatchControls): a COSM
// off whose store says Through keeps its switch and Type as stored, where the controls would give it a type of theirs
constexpr int kCosmSwitch = 0x3e7, kCosmType = 0x3e8, kCosmThrough = 0;
XtPatchIds keepingThrough(XtPatchIds ids, const XtSynth::Controls& c, const std::vector<uint8_t>& store) {
    for (int k = 0; k < 2; ++k)
        if (!c.cosm[k].on && xtPatchValue(store, kCosmType, 0, k) == kCosmThrough)
            for (int id : { kCosmSwitch, kCosmType }) ids.erase(id + (k ? kXtCosm2Ids : 0));
    return ids;
}

// The effects as the plugin holds them, by the store's types (`typed`: their types' parameters, after the blocks' own
// have set the types): a knob's parameter, else the processor's own value, the store's when it has none
XtPatchIds effectIds(const VSynthEmuProcessor& p, const std::vector<uint8_t>& store, bool typed) {
    XtPatchIds ids;
    for (const XtPatchFx& fx : xtPatchEffects(store)) {
        if ((fx.id >= kXtDescIds) != typed) continue;
        const char* knob = VSynthEmuProcessor::fxKnobOf(fx.id);
        const auto* prm = knob ? p.apvts.getParameter(knob) : nullptr;
        ids[fx.id] = prm ? juce::roundToInt(prm->convertFrom0to1(prm->getValue())) : p.fxParam(fx.id, fx.value);
    }
    return ids;
}

// [FW] an OSC's Loop switch (OSC 1's; OSC 2's kOscStride past it).  A wave whose loop record says the switch doesn't
// decide loops whatever it says (vplp +0x1a, the converter's tail 0x894e4030), and the plugin shows that by turning the
// switch on (xtWaveOntoControls): so on such a wave the save keeps the patch's own switch, as the XT would (v603)
constexpr int kLoopSwitch = 0x2d3;
XtPatchIds keepingLoops(XtPatchIds ids, const VSynthEmuProcessor& p) {
    for (int osc = 0; osc < 2; ++osc) {
        XtWaveFile wave;
        const juce::File file = oscWaveFile(p, osc);
        if (file.existsAsFile() && readVSynthWave(file, wave) && !wave.loopSwitchDecides)
            ids.erase(kLoopSwitch + kOscStride * osc);
    }
    return ids;
}

XtPatchIds nameIds(const juce::String& name) {
    XtPatchIds ids;
    const juce::String padded = name.substring(0, kNameChars).paddedRight(' ', kNameChars);
    for (int k = 0; k < kNameChars; ++k) ids[kNameFirst + k] = static_cast<int>(padded[k]);
    return ids;
}
} // namespace

juce::File oscWaveFile(const VSynthEmuProcessor& p, int osc) {
    const juce::String path = p.apvts.state[osc ? "loadedFilePath2" : "loadedFilePath"].toString();
    return path.isEmpty() ? juce::File() : juce::File(path);
}

bool patchFileOf(const VSynthEmuProcessor& p, const juce::String& name, std::vector<uint8_t>& file, std::string* why) {
    return patchFileOf(p, name, PatchWaveSlots {}, file, why);
}

bool patchFileOf(const VSynthEmuProcessor& p, const juce::String& name, const PatchWaveSlots& slots,
                 std::vector<uint8_t>& file, std::string* why) {
    if (!xtSynthRomReady()) return fail(why, "a patch needs the firmware image");
    std::vector<uint8_t> store = p.keptStore();
    const uint8_t none = 0;   // with no patch loaded, the firmware's default patch under no file's bytes
    if (store.empty() && !xtFwPatchStore(&none, 0, store)) return fail(why, "the firmware's default patch failed");
    const XtSynth::Controls c = p.currentControls();
    XtPatchIds tone = keepingLoops(
        keepingThrough(xtArpPatchIds(xtWithMsmIds(xtPatchIds(c), c.msm), c.arp, c.arpPattern), c, store), p);
    for (size_t osc = 0; osc < slots.size(); ++osc)
        if (slots[osc]) tone[kWaveNumber + kOscStride * static_cast<int>(osc)] = *slots[osc];
    // The first bank first, the types among it; then the typed values, by the types the store now holds
    const XtPatchIds first = merged(merged(tone, nameIds(name)), effectIds(p, store, false));
    const bool set = setChanged(store, first, false, why)
                     && setChanged(store, merged(tone, effectIds(p, store, true)), true, why);
    return set && xtFwPatchFile(store, file, why);
}

bool savePatch(const VSynthEmuProcessor& p, const juce::String& name, const juce::File& file, std::string* why) {
    std::vector<uint8_t> bytes;
    if (!patchFileOf(p, name, bytes, why)) return false;
    return file.replaceWithData(bytes.data(), bytes.size()) || fail(why, "the file can't be written");
}
