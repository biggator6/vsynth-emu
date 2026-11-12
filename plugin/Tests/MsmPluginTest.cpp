// The Multi Step Modulator's plugin side (v193), through the processor.  Run by VSynthEmuArpPluginTest after the
// arpeggio's checks, so mutate_panel.py's arpplugin runs reach it too:
// - its 98 parameters against the firmware: the last 98 of the list, in the XT's order; each name [FW 0x89790338],
//   range and default (its descriptor, through xtParamDesc) and each value's text (the firmware's routine,
//   XtFxValueText), End Point and Zone from 1 as the manual has them;
// - the parameters into the controls the processor plays from, each moved from its default to its own field;
// - every patch of the disk image with the MSM on, through applyPatch: its settings on the parameters as the firmware's
//   reads have them (xtMsmSettingsOf); one through loadPatch, and its session in another processor;
// - a note through processBlock with the MSM on: sample for sample XtSynth's render on the processor's own controls,
//   and not what it plays with the MSM off;
// - a patch loaded again while the MSM runs stops it, as the XT's load does (XtSynth::msmPatch): sample for sample
//   XtSynth told the same, and not XtSynth left running.
#include "MsmPluginTest.h"
#include "TestAgainstSynth.h"
#include "TestChecks.h"
#include "TestControls.h"
#include "TestFirmware.h"
#include "../Source/PluginMsm.h"
#include "../Source/XtPatch.h"

namespace {

constexpr double kRate = 48000.0;
constexpr int kBlock = 480;                   // 10 ms
constexpr int kKey = 60;
constexpr int kMsmParams = kXtMsmLastId - kXtMsmFirstId + 1;
using Knob = std::pair<const char*, float>;

// The plugin's ID and name the test expects for an XT ID, by its own reading of the scheme: the firmware's name, but the
// Loop's, "Track Loop Switch" in every track, given its track's number
juce::String expectedId(int fw) {
    static const char* const kTrack[] = { "Switch", "Loop", "Type", "Grid", "EndPoint", "Direction", "Zone", "Dest" };
    if (fw < 0x48f) return fw == 0x48d ? "msmSwitch" : "msmKeySync";
    const int n = (fw - 0x48f) / 24, i = (fw - 0x48f) % 24;
    return "msm" + juce::String(n + 1) + (i < 8 ? juce::String(kTrack[i]) : "Step" + juce::String(i - 7));
}
juce::String expectedName(Sh4& image, int fw) {
    const juce::String name = firmwareName(image, fw), track = juce::String((fw - 0x48f) / 24 + 1);
    return "MSM " + (name == "Track Loop Switch" ? "Track " + track + " Loop Switch" : name);
}
bool countsFromOne(int fw) {   // End Point and Zone
    return fw >= 0x48f && ((fw - 0x48f) % 24 == 4 || (fw - 0x48f) % 24 == 6);
}

// The last 98 of the list, in the XT's order, each against the firmware: every value's text but the toggles' (Off, On)
void msmAgainstFirmware(TestChecks& check, VSynthEmuProcessor& p, const juce::File& rom) {
    Firmware fw(rom);
    check(fw.text.ok(), "the firmware's text routine should run");
    const auto& list = p.getParameters();
    for (int id = kXtMsmFirstId; id <= kXtMsmLastId; ++id) {
        auto* prm = dynamic_cast<juce::RangedAudioParameter*>(list[list.size() - kMsmParams + id - kXtMsmFirstId]);
        const juce::String want = expectedId(id);
        check(prm && prm->paramID == want && msmParamId(id) == want, want.toStdString() + " should be in its place");
        const bool texts = !dynamic_cast<juce::AudioParameterBool*>(prm);
        paramAgainstFirmware(check, prm, { id, expectedName(fw.image, id), texts, countsFromOne(id) }, fw.text);
    }
}

// A value of the parameter's range that isn't its default, a different step from it for each
int movedFrom(VSynthEmuProcessor& p, int fw) {
    auto* prm = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(msmParamId(fw)));
    const auto& r = prm->getNormalisableRange();
    const int lo = (int)r.start, span = (int)r.end - lo + 1;
    const int def = (int)std::lround(prm->convertFrom0to1(prm->getDefaultValue()));
    return lo + (def - lo + 1 + (fw - kXtMsmFirstId) % (span - 1)) % span;
}

// Each parameter moved reaches its own field of the controls; a new processor's are the object's defaults
void msmIntoControls(TestChecks& check, const juce::File& rom) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    check(p.currentControls().msm == XtMsmSettings {}, "a new processor's MSM should be the object's defaults");
    msmAgainstFirmware(check, p, rom);
    XtMsmSettings want;
    for (int fw = kXtMsmFirstId; fw <= kXtMsmLastId; ++fw) {
        const int v = movedFrom(p, fw);
        setParam(p, msmParamId(fw), (float)v);
        *xtMsmField(want, fw) = v;
    }
    const XtMsmSettings got = p.currentControls().msm;
    for (int fw = kXtMsmFirstId; fw <= kXtMsmLastId; ++fw) {
        const std::string what = expectedId(fw).toStdString() + " should reach its own field";
        check(xtMsmValue(got, fw) == xtMsmValue(want, fw), what);
    }
}

// The disk image's patches with the MSM on
struct MsmPatch {
    std::string name;
    std::vector<uint8_t> store, file;
};
std::vector<MsmPatch> msmPatches(const juce::File& rom) {
    std::vector<MsmPatch> out;
    XtDiskImage disk;
    if (!disk.open(rom.getSiblingFile("V-SynthXTDiskImage.bin.0.bin").getFullPathName().toStdString())) return out;
    for (const auto& e : disk.list("Internal.prj/Patch")) {
        XtPatchFile patch;
        const std::vector<uint8_t> bytes = disk.read(e);
        if (xtPatchRead(bytes.data(), bytes.size(), patch) && xtPatchValue(patch.store, kXtMsmFirstId) != 0)
            out.push_back({ patch.name, patch.store, bytes });
    }
    return out;
}

// Each through applyPatch; the last through loadPatch, and its session in another processor
void msmPatchesAndSession(TestChecks& check, const juce::File& rom, const juce::File& work,
                          const std::vector<MsmPatch>& patches) {
    check(patches.size() == 55, "the disk image's 55 patches with the MSM on, not " + std::to_string(patches.size()));
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    for (const MsmPatch& m : patches) {
        p.applyPatch(m.name, m.store, 0, juce::File());
        check(p.currentControls().msm == xtMsmSettingsOf(m.store), m.name + ": its MSM's settings on the parameters");
        check(!p.getPatchNote().contains("Multi Step"), m.name + ": the MSM said to be missing");
    }
    if (patches.empty()) return;
    const juce::File file = work.getChildFile("msm.p00");
    file.replaceWithData(patches.back().file.data(), patches.back().file.size());
    VSynthEmuProcessor q;
    q.setRomFile(rom);
    const bool loaded = q.loadPatch(file);
    check(loaded && q.currentControls().msm == xtMsmSettingsOf(patches.back().store), "one through loadPatch");
    juce::MemoryBlock state;
    q.getStateInformation(state);
    VSynthEmuProcessor r;
    r.setStateInformation(state.getData(), (int)state.getSize());
    check(r.currentControls().msm == q.currentControls().msm, "a session's MSM settings should come back");
}

// A note through processBlock and through XtSynth on the processor's controls, block by block: at block `at` the note
// let go, after `between` on the processor and `ref` on XtSynth (a patch's load); at `again` another
struct Run {
    int blocks = 210;
    int at = -1, again = -1;
    std::function<void(VSynthEmuProcessor&)> between;
    std::function<void(XtSynth&)> ref;
};
struct Played {
    std::vector<float> left;   // the processor's
    float most = 0.0f;         // the most a sample of it differs from XtSynth's
};
Played noteAgainstSynth(VSynthEmuProcessor& p, const Run& run) {
    AgainstSynth both(p, kRate, kBlock);
    Played h;
    for (int b = 0; b < run.blocks; ++b) {
        const bool off = b == run.at, on = b == 0 || b == run.again;
        if (off) run.between(p);
        if (off) run.ref(*both.ref);
        juce::MidiBuffer midi;
        if (off) midi.addEvent(juce::MidiMessage::noteOff(1, kKey), 0);
        if (on) midi.addEvent(juce::MidiMessage::noteOn(1, kKey, (juce::uint8)100), 0);
        both.block(midi, [off, on](XtSynth& ref) {
            if (off) ref.noteOff(kKey, 0.0f);
            if (on) ref.noteOn(kKey, 100 / 127.0f);
        });
        const float* l = both.buf.getReadPointer(0);
        h.left.insert(h.left.end(), l, l + kBlock);
    }
    h.most = both.most;
    return h;
}

float apart(const std::vector<float>& a, const std::vector<float>& b) {
    float most = 0.0f;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) most = std::max(most, std::fabs(a[i] - b[i]));
    return most;
}

// The analog saw, track 1 on the coarse tune at quarters, +0 and +63 (24 semitones) by turns
void msmHeard(TestChecks& check, const juce::File& rom) {
    const auto withMsm = [&rom](bool on) {
        VSynthEmuProcessor p;
        p.setRomFile(rom);
        const Knob knobs[] = { { "oscType", 0 }, { "ampAttack", 0 }, { "ampRelease", 0 },
                               { "msmSwitch", on ? 1.0f : 0.0f }, { "msm1Grid", 0 } };
        for (const auto& [id, v] : knobs) setParam(p, id, v);
        for (int s = 1; s <= 16; ++s) setParam(p, "msm1Step" + juce::String(s), s % 2 ? 0.0f : 63.0f);
        return noteAgainstSynth(p, Run {});
    };
    const Played on = withMsm(true), off = withMsm(false);
    const std::string by = std::to_string(on.most);
    check(on.most == 0.0f, "the MSM on: the processor should play what XtSynth does, off by " + by);
    check(off.most == 0.0f, "the MSM off: the same, off by " + std::to_string(off.most));
    check(apart(on.left, off.left) > 1e-3f, "the MSM should be heard");
}

// A store's value put as the firmware's read gives it (xtPatchRaw's layout the other way)
void putStore(std::vector<uint8_t>& store, int id, int value) {
    const XtParamDesc* d = xtParamDesc(id);
    const int width = std::abs(d->width), total = width + d->bit, bytes = (total + 7) / 8;
    const size_t at = (size_t)(xtPatchSection(id) + d->offset);
    uint32_t word = 0;
    for (int k = 0; k < bytes; ++k) word = word << 8 | store[at + (size_t)k];
    const int shift = bytes * 8 - total;
    const uint32_t mask = ((1u << width) - 1) << shift;
    word = (word & ~mask) | (((uint32_t)(value + d->centre) << shift) & mask);
    for (int k = bytes - 1; k >= 0; --k, word >>= 8) store[at + (size_t)k] = (uint8_t)word;
}

// A patch of the image's with the MSM on and one zone, its OSC 1 the analog oscillator, Key Sync OFF and the arpeggio
// off: so the MSM runs on from note to note, and only a patch's load stops it
MsmPatch runningPatch(TestChecks& check, const std::vector<MsmPatch>& patches) {
    MsmPatch m = patches.front();
    for (const MsmPatch& c : patches)
        if (xtPatchZones(c.store) == 1 && xtPatchValue(c.store, 0x2a5) == 0) {
            m = c;
            break;
        }
    const std::pair<int, int> set[] = { { 0x2a4, 1 }, { 0x2a5, 0 }, { 0x42e, 0 }, { kXtMsmFirstId + 1, 0 } };
    for (const auto& [id, v] : set) putStore(m.store, id, v);
    for (const auto& [id, v] : set) check(xtPatchValue(m.store, id) == v, m.name + ": its store's value put");
    return m;
}

// The patch loaded again 1 s into a note, the note let go, and another 50 ms on
void msmStoppedByPatch(TestChecks& check, const juce::File& rom, const MsmPatch& m) {
    const auto run = [&](bool told) {
        VSynthEmuProcessor p;
        p.setRomFile(rom);
        p.applyPatch(m.name, m.store, 0, juce::File());
        const XtSynth::Controls c = p.currentControls();
        const auto load = [&m](VSynthEmuProcessor& q) { q.applyPatch(m.name, m.store, 0, juce::File()); };
        const auto same = [c, told](XtSynth& s) {   // its waves and zones, which applyPatch gives the synth, none here
            s.setSample(nullptr, 0);
            s.setSample(nullptr, 1);
            s.setZones(nullptr);
            if (told) s.msmPatch();
            s.setControls(c);
        };
        return noteAgainstSynth(p, Run { 210, 100, 105, load, same }).most;
    };
    const std::string name = m.name + ": ";
    const float told = run(true), left = run(false);
    check(told == 0.0f, name + "a patch's load should stop the MSM as XtSynth's does, off by " + std::to_string(told));
    check(left > 1e-4f, name + "and the stop should be heard: XtSynth left running is off by " + std::to_string(left));
}

} // namespace

int msmPluginTest(const juce::File& rom, const juce::File& work) {
    TestChecks check { "msmplugin" };
    msmIntoControls(check, rom);
    const std::vector<MsmPatch> patches = msmPatches(rom);
    msmPatchesAndSession(check, rom, work, patches);
    msmHeard(check, rom);
    if (!patches.empty()) msmStoppedByPatch(check, rom, runningPatch(check, patches));
    return check.summary();
}
