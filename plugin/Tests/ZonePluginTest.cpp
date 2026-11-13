// The zones' plugin side (v195), through the processor:
// - the sixteen Range Upper keys against the firmware: each name [FW 0x89790338], range and default [FW 0x897a167c] and
//   each value's text (the firmware's routine, XtFxValueText);
// - each into the controls the processor plays from, to its own zone; a new processor's all 127;
// - a kit of the disk image's through loadPatch: its splits on the parameters as its store holds them, and what each zone
//   plays said for the Zones tab; a patch of one zone loads its zones past its splits too;
// - a session: the splits back in another processor; one saved before v195 takes them from the store it kept, or none;
// - notes through processBlock against XtSynth on the processor's own controls, on the analog saw with no patch: a split
//   moved under a sounding note leaves it sounding, the next key above the split plays an init zone (silent) and one below
//   it plays zone 1.
// Run by VSynthEmuArpPluginTest after the arpeggio's and the MSM's checks.
#include "ZonePluginTest.h"
#include "TestAgainstSynth.h"
#include "TestChecks.h"
#include "TestControls.h"
#include "TestFirmware.h"
#include "TestOnsets.h"
#include "TestPatches.h"
#include "../Source/PluginParamIds.h"
#include <algorithm>
#include <optional>
#include <ranges>
#include <span>
#include <string>

namespace {

constexpr int kFirstUpper = 0x124;            // zone 1's Range Upper; zone z's is 0x124 + z
constexpr double kRate = 48000.0;
constexpr int kBlock = 480;                   // 10 ms

// The last sixteen of the list, in the zones' order, each split's name, range, default and texts against the firmware's
void splitsAgainstFirmware(TestChecks& check, VSynthEmuProcessor& p, const juce::File& rom) {
    Firmware fw(rom);
    check(fw.text.ok(), "the firmware's text routine should run");
    const auto& list = p.getParameters();
    for (int z = 0; z < kXtZones; ++z) {
        auto* prm = dynamic_cast<juce::RangedAudioParameter*>(list[list.size() - kXtZones + z]);
        check(prm && prm->paramID == Params::zoneUpperId(z), "zone " + std::to_string(z + 1) + "'s split in its place");
        const int id = kFirstUpper + z;
        paramAgainstFirmware(check, prm, { id, firmwareName(fw.image, id), true, false }, fw.text);
    }
}

bool all127(const XtSynth::Controls& c) {
    return std::ranges::all_of(c.zoneUpper, [](int u) { return u == 127; });
}

// Each split moved reaches its own zone's field of the controls; a new processor's are the descriptors' 127
void splitsIntoControls(TestChecks& check, const juce::File& rom) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    check(all127(p.currentControls()), "a new processor's splits should all be 127");
    splitsAgainstFirmware(check, p, rom);
    for (int z = 0; z < kXtZones; ++z) setParam(p, Params::zoneUpperId(z), (float)(7 * z + 3));
    const XtSynth::Controls c = p.currentControls();
    for (int z = 0; z < kXtZones; ++z)
        check(c.zoneUpper[z] == 7 * z + 3, "zone " + std::to_string(z + 1) + "'s split should reach its own field");
}

// The first patch of the image's with more than two zones (a kit), or with one zone when `one`
std::optional<ImagePatch> zonesPatch(const juce::File& rom, bool one) {
    for (ImagePatch& m : imagePatches(rom)) {
        const int zones = xtPatchZones(m.store);
        if (one ? zones == 1 : zones > 2) return m;
    }
    return std::nullopt;
}

bool splitsOf(const XtSynth::Controls& c, const std::vector<uint8_t>& store) {
    for (int z = 0; z < kXtZones; ++z)
        if (c.zoneUpper[z] != xtPatchValue(store, kFirstUpper + z)) return false;
    return true;
}

// A session of `p` in a new processor, and as one saved before v195: no splits, then no store either
void sessions(TestChecks& check, VSynthEmuProcessor& p, const ImagePatch& kit) {
    juce::MemoryBlock state;
    p.getStateInformation(state);
    VSynthEmuProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    check(splitsOf(q.currentControls(), kit.store), "a session's splits should come back");
    const juce::MemoryBlock before = sessionBefore(state, "zoneUpper", nullptr);
    VSynthEmuProcessor r;
    r.setStateInformation(before.getData(), (int)before.getSize());
    check(splitsOf(r.currentControls(), kit.store), "a session from before v195 should take its splits from its store");
    const juce::MemoryBlock bare = sessionBefore(state, "zoneUpper", "patchStore");
    q.setStateInformation(bare.getData(), (int)bare.getSize());
    check(all127(q.currentControls()), "and one with no store none, though a kit's came before");
}

// A kit through loadPatch, then its sessions; then a patch of one zone, with its other zones
void patches(TestChecks& check, const juce::File& rom, const juce::File& work) {
    const std::optional<ImagePatch> kit = zonesPatch(rom, false), single = zonesPatch(rom, true);
    if (!kit || !single) {
        check(false, "a kit and a patch of one zone of the image's, beside the ROM");
        return;
    }
    const auto loaded = loadedWith(check, rom, *kit, work.getChildFile("Patch/kit.p00"));
    VSynthEmuProcessor& p = *loaded;
    const std::string name = kit->name + ": ";
    check(splitsOf(p.currentControls(), kit->store), name + "its splits on the parameters, as its store holds them");
    check(p.patchZones() == xtPatchZones(kit->store), name + "its zones counted");
    check(p.zonePlays().size() == kXtZones && p.zonePlays()[1].isNotEmpty(), name + "what each zone plays said");
    sessions(check, p, *kit);
    const juce::File one = work.getChildFile("Patch/one.p00");
    writeTestFile(one, single->file);
    check(p.loadPatch(one), "a patch of one zone should load");
    check(p.patchZones() == 1 && p.zonePlays().size() == kXtZones && p.zonePlays()[1].isNotEmpty(),
          single->name + ": its zones past its splits loaded too, for a split moved to reach them");
}

// The note run: zone 1's split moved to 60 at kMove, under key 72; then a key above the split and one below it
constexpr int kMove = 10, kOff = 25, kAbove = 30, kBelow = 50, kEnd = 70;
struct Key {
    int block, key;
    bool on;
};
constexpr Key kKeys[] = { { 0, 72, true }, { kOff, 72, false }, { kAbove, 76, true }, { kBelow, 48, true } };

void playOn(XtSynth& ref, const juce::MidiBuffer& midi) {
    for (const auto m : midi) {
        const juce::MidiMessage msg = m.getMessage();
        if (msg.isNoteOn()) ref.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        if (msg.isNoteOff()) ref.noteOff(msg.getNoteNumber());
    }
}

bool all(const std::vector<float>& rms, int from, int to, bool loud) {
    const auto span = std::span(rms).subspan((size_t)from, (size_t)(to - from));
    return std::ranges::all_of(span, [loud](float x) { return loud ? x > 0.0f : x == 0.0f; });
}

// The analog saw with no patch, every block against XtSynth on the processor's own controls, given them at the same block
void splitUnderNotes(TestChecks& check, const juce::File& rom) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    for (const char* id : { "oscType", "ampAttack", "ampRelease" }) setParam(p, id, 0.0f);
    AgainstSynth both(p, kRate, kBlock);
    std::vector<float> rms;
    juce::MidiBuffer midi;
    for (int b = 0; b < kEnd; ++b) {
        midi.clear();
        const bool move = b == kMove;
        if (move) setParam(p, Params::zoneUpperId(0), 60.0f);
        for (const Key& k : kKeys | std::views::filter([b](const Key& x) { return x.block == b; }))
            midi.addEvent(k.on ? juce::MidiMessage::noteOn(1, k.key, (juce::uint8)100)
                               : juce::MidiMessage::noteOff(1, k.key), 0);
        both.block(midi, [&](XtSynth& ref) {
            if (move) ref.setControls(p.currentControls());
            playOn(ref, midi);
        });
        rms.push_back(frameRms(both.buf.getReadPointer(0), both.buf.getReadPointer(1), kBlock));
    }
    check(p.currentControls().zoneUpper[0] == 60, "zone 1's split should be on the controls");
    check(both.most == 0.0f, "the processor should play what XtSynth does, off by " + std::to_string(both.most));
    check(all(rms, kMove + 1, kOff, true), "a note sounding when the split moved under it should go on sounding");
    check(all(rms, kAbove + 1, kBelow, false),
          "a key above the split, with no patch, should play nothing: an init zone");
    check(all(rms, kBelow + 1, kEnd, true), "a key below it should play zone 1");
}

} // namespace

int zonePluginTest(const juce::File& rom, const juce::File& work) {
    TestChecks check { "zoneplugin" };
    splitsIntoControls(check, rom);
    patches(check, rom, work);
    splitUnderNotes(check, rom);
    return check.summary();
}
