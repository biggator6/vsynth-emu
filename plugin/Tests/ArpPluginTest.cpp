// The arpeggio's plugin side (v190), through the processor:
// - its ten parameters against the firmware: each name [FW 0x89790338], range and default (its descriptor, through
//   xtParamDesc) and each value's text (the firmware's routine, XtFxValueText), End Point shown as its step (1…32, the
//   manual's);
// - the parameters into the controls the processor plays from, each moved from its default; a new processor's pattern
//   the firmware's default patch's, an initialized patch's (v469, the user's d-6), which plays;
// - a patch of the disk image's with the arpeggio on, through loadPatch: its settings on the parameters and its pattern,
//   as the firmware's reads have them (xtArpSettingsOf, xtArpPatternOf), and no longer said to be missing;
// - a session: settings and pattern back in another processor; one saved before v190 brings the XT's default pattern;
// - a chord through processBlock with the arpeggio on and no patch, on the new processor's pattern: XtSynth's render on
//   the processor's own controls, sample for sample, a note each quarter; the Switch off, the chord once.
// Then the Multi Step Modulator's plugin side (MsmPluginTest.cpp, v193), the zones' (v195) and the VC-2's parameters
// (Vc2ParamPluginTest.cpp, v451), whose checks this program runs too; `vc2ids`, the VC-2 part's child run.
// `params` lists every parameter instead: its place, ID, name, range, default and its text at five points, what a host
// sees of it, so a change to how the list is made can be shown to leave it as it was (v190's split of PluginProcessor.cpp).
// Run: VSynthEmuArpPluginTest <rom.2.bin> [params].  Exit 0 if every check holds.
#include "MsmPluginTest.h"
#include "StartOffsetPluginTest.h"
#include "Vc2ParamPluginTest.h"
#include "Vc2PatchPluginTest.h"
#include "ZonePluginTest.h"
#include "TestChecks.h"
#include "TestControls.h"
#include "TestFirmware.h"
#include "TestMain.h"
#include "TestAgainstSynth.h"
#include "TestOnsets.h"
#include "TestPatches.h"
#include "../Source/PluginProcessor.h"
#include "../Source/Sh4Interp.h"
#include "../Source/XtFiles.h"
#include "../Source/XtFirmware.h"
#include "../Source/XtPatch.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>

namespace {

// The plugin's ID for each of the XT's
struct ArpId {
    const char* id;
    int fw;
};
constexpr ArpId kArpIds[] = { { "arpSwitch", 0x42e },      { "arpHold", 0x42f },        { "arpMotif", 0x430 },
                              { "arpDuration", 0x431 },    { "arpShuffleRate", 0x432 }, { "arpShuffleRes", 0x433 },
                              { "arpKeyVelocity", 0x434 }, { "arpOctave", 0x435 },      { "arpGrid", 0x436 },
                              { "arpEndPoint", 0x437 } };
constexpr int kEndPointId = 0x437;
constexpr double kRate = 48000.0;
constexpr int kBlock = 480;                   // 10 ms
constexpr int kChord[] = { 60, 64, 67 };
using Setting = std::pair<const char*, float>;

// Each one's name, range, default and texts against the firmware's
void againstFirmware(TestChecks& check, VSynthEmuProcessor& p, const juce::File& rom) {
    Firmware fw(rom);
    check(fw.text.ok(), "the firmware's text routine should run");
    for (const ArpId& a : kArpIds) {
        auto* prm = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(a.id));
        const bool texts = prm && prm->getNumSteps() > 2;
        paramAgainstFirmware(check, prm, { a.fw, firmwareName(fw.image, a.fw), texts, a.fw == kEndPointId }, fw.text);
    }
}

// The pattern before any patch (v469, the user's d-6) as the firmware has it: its default patch's, which its PATCH
// initialize writes (line 1 at velocity 100 on every step), from the firmware's own patch routines on no file's bytes
XtArpPattern firmwareDefaultPattern() {
    std::vector<uint8_t> store;
    const uint8_t none = 0;
    return xtFwPatchStore(&none, 0, store) ? xtArpPatternOf(store) : XtArpPattern {};
}

// Each parameter moved from its default reaches the controls; a new processor's are the arpeggio object's own
void intoControls(TestChecks& check, const juce::File& rom) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    check(p.currentControls().arp == XtArpSettings {}, "a new processor's arpeggio should be the object's defaults");
    check(p.arpPattern() == firmwareDefaultPattern(), "and its pattern the firmware's default patch's, initialized");
    check(!xtArpPatternSilent(p.arpPattern()), "which plays");
    againstFirmware(check, p, rom);
    const Setting moved[] = { { "arpSwitch", 1 },       { "arpHold", 1 },       { "arpMotif", 6 },
                              { "arpDuration", 37 },    { "arpShuffleRate", 71 }, { "arpShuffleRes", 1 },
                              { "arpKeyVelocity", 90 }, { "arpOctave", -2 },    { "arpGrid", 5 },
                              { "arpEndPoint", 11 } };
    for (const auto& [id, v] : moved) setParam(p, id, v);
    XtArpSettings want;
    want.on = true;
    want.hold = 1;
    want.motif = 6;
    want.duration = 37;
    want.shuffleRate = 71;
    want.shuffleRes = 1;
    want.keyVelocity = 90;
    want.octave = -2;
    want.grid = 5;
    want.endPoint = 11;
    check(p.currentControls().arp == want, "the parameters should reach the controls, each to its own field");
}

// A patch of the image's with the arpeggio on, a pattern that plays and settings away from the defaults
std::optional<ImagePatch> arpPatch(const juce::File& rom) {
    for (ImagePatch& m : imagePatches(rom)) {   // TestPatches.h (v195)
        const XtArpSettings s = xtArpSettingsOf(m.store);
        if (s.on && !xtArpPatternSilent(xtArpPatternOf(m.store)) && s.motif != 0 && s.grid != 3) return m;
    }
    return std::nullopt;
}

// Through loadPatch, then a session of it, and one from before v190
void patchAndSession(TestChecks& check, const juce::File& rom, const juce::File& work) {
    const std::optional<ImagePatch> patch = arpPatch(rom);
    if (!patch) {
        check(false, "a patch of the image's with the arpeggio on, beside the ROM");
        return;
    }
    const auto loaded = loadedWith(check, rom, *patch, work.getChildFile("Patch/arp.p00"));   // TestPatches.h (v195)
    VSynthEmuProcessor& p = *loaded;
    const std::string name = patch->name + ": ";
    check(p.currentControls().arp == xtArpSettingsOf(patch->store), name + "its arpeggio's settings on the parameters");
    check(p.arpPattern() == xtArpPatternOf(patch->store), name + "its pattern");
    check(!p.getPatchNote().contains("arpeggio"), name + "not said to be missing: " + p.getPatchNote().toStdString());

    juce::MemoryBlock state;
    p.getStateInformation(state);
    VSynthEmuProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    check(q.currentControls().arp == p.currentControls().arp, "a session's arpeggio settings should come back");
    check(q.arpPattern() == p.arpPattern(), "and its pattern");
    const juce::MemoryBlock before = sessionBefore(state, "arp", "arpPattern");   // no pattern, no arpeggio's parameters
    q.setStateInformation(before.getData(), (int)before.getSize());
    check(q.arpPattern() == xtArpDefaultPattern(), "a session from before v190 should bring the XT's default pattern");
    VSynthEmuProcessor r;
    r.setStateInformation(before.getData(), (int)before.getSize());
    check(r.currentControls().arp == XtArpSettings {}, "and the arpeggio's default settings to a new processor");
}

// A chord through processBlock, frame levels and the most any sample differs from XtSynth on the same controls
struct Heard {
    std::vector<float> rms;
    float most = 0.0f;
};
Heard play(VSynthEmuProcessor& p, double seconds) {
    AgainstSynth both(p, kRate, kBlock);
    juce::MidiBuffer midi;
    Heard h;
    for (int b = 0; b < (int)(seconds * kRate / kBlock); ++b) {
        midi.clear();
        for (const int k : kChord)
            if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, k, (juce::uint8)100), 0);
        both.block(midi, [b](XtSynth& ref) {
            for (const int k : kChord)
                if (b == 0) ref.noteOn(k, 100 / 127.0f);
        });
        h.rms.push_back(frameRms(both.buf.getReadPointer(0), both.buf.getReadPointer(1), kBlock));
    }
    h.most = both.most;
    return h;
}

// The analog saw, UP on quarters at 120 BPM, on a new processor's pattern, no patch's (v469: an initialized patch's, line
// 1 at velocity 100 on every step): live keys, a note each quarter, as XtSynth plays it
void throughProcessor(TestChecks& check, const juce::File& rom, bool on) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    const Setting set[] = { { "oscType", 0 }, { "ampAttack", 0 }, { "ampRelease", 0 },
                            { "arpSwitch", on ? 1.0f : 0.0f }, { "arpGrid", 0 }, { "arpDuration", 25 } };
    for (const auto& [id, v] : set) setParam(p, id, v);
    const Heard h = play(p, 2.1);
    const std::vector<double> at = onsetsMs(h.rms, 1000.0 * kBlock / kRate);
    const std::string which = on ? "the Switch on: " : "the Switch off: ";
    check(h.most == 0.0f, which + "the processor should play what XtSynth does, off by " + std::to_string(h.most));
    const std::string heard = on ? "a note each quarter" : "the chord once";
    check((int)at.size() == (on ? 5 : 1), which + heard + ", heard at" + said(at));
}

int arpPluginTest(const juce::File& rom, const juce::File& work) {
    TestChecks check { "arpplugin" };
    intoControls(check, rom);
    patchAndSession(check, rom, work);
    throughProcessor(check, rom, true);
    throughProcessor(check, rom, false);
    const int arp = check.summary();
    // the MSM's (v193), the zones' (v195), the VC-2's parameters (v451), a .p02 onto them (v452), Start Offset's text (v478)
    return msmPluginTest(rom, work) | zonePluginTest(rom, work) | vc2ParamPluginTest(rom, work)
           | vc2PatchPluginTest(rom, work) | startOffsetPluginTest(rom, work) | arp;
}

// Every parameter as a host sees it, one a line: rom.4 in from beside rom.2 first, as an instance with both finds it
int paramList(const juce::File& rom, const juce::File&) {
    xtSynthSetVc2Rom(rom.getSiblingFile("V-SynthXT.rom.4.bin").getFullPathName().toStdString());
    VSynthEmuProcessor p;
    int place = 0;
    for (auto* base : p.getParameters()) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(base);
        if (!r) continue;
        const auto& range = r->getNormalisableRange();
        std::printf("%d %s \"%s\" %g..%g by %g skew %g, default %g, %d steps, label \"%s\":", place++,
                    r->paramID.toRawUTF8(), r->name.toRawUTF8(), range.start, range.end, range.interval, range.skew,
                    r->convertFrom0to1(r->getDefaultValue()), r->getNumSteps(), r->getLabel().toRawUTF8());
        for (const float at : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            std::printf(" \"%s\"", r->getText(at, 64).toRawUTF8());
        std::printf("\n");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const char* mode = argc > 2 ? argv[2] : "";
    if (std::strcmp(mode, "vc2ids") == 0) return testMain(argc, argv, "arpplugin", vc2ParamStandIns);   // v451's child
    return testMain(argc, argv, "arpplugin", std::strcmp(mode, "params") == 0 ? paramList : arpPluginTest);
}
