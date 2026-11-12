// The arpeggio's plugin side (v190), through the processor:
// - its ten parameters against the firmware: each name [FW 0x89790338], range and default (its descriptor, through
//   xtParamDesc) and each value's text (the firmware's routine, XtFxValueText), End Point shown as its step (1…32, the
//   manual's);
// - the parameters into the controls the processor plays from, each moved from its default;
// - a patch of the disk image's with the arpeggio on, through loadPatch: its settings on the parameters and its pattern,
//   as the firmware's reads have them (xtArpSettingsOf, xtArpPatternOf), and no longer said to be missing;
// - a session: settings and pattern back in another processor; one saved before v190 brings the XT's default pattern;
// - a chord through processBlock with the arpeggio on: XtSynth's render on the processor's own controls, sample for
//   sample, a note each quarter; the Switch off, the chord once.
// Then the Multi Step Modulator's plugin side (MsmPluginTest.cpp, v193), whose checks this program runs too.
// `params` lists every parameter instead: its place, ID, name, range, default and its text at five points, what a host
// sees of it, so a change to how the list is made can be shown to leave it as it was (v190's split of PluginProcessor.cpp).
// Run: VSynthEmuArpPluginTest <rom.2.bin> [params].  Exit 0 if every check holds.
#include "MsmPluginTest.h"
#include "TestChecks.h"
#include "TestControls.h"
#include "TestFirmware.h"
#include "TestMain.h"
#include "TestAgainstSynth.h"
#include "TestOnsets.h"
#include "../Source/PluginProcessor.h"
#include "../Source/Sh4Interp.h"
#include "../Source/XtFiles.h"
#include "../Source/XtPatch.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <span>

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

// The default pattern against the descriptors: each line's State (ID 0x44a) and Note (0x44b), each step's cell (0x44d)
bool descriptorsPattern(const XtArpPattern& p) {
    const auto def = [](int id) {
        const XtParamDesc* d = xtParamDesc(id);
        return d ? d->def - d->centre : -1;
    };
    const int line = (def(0x44a) << 7) | def(0x44b), cell = def(0x44d);
    const auto lines = std::span(p.line);
    const auto cells = std::span(&p.cell[0][0], sizeof p.cell);
    return std::ranges::all_of(lines, [&](int l) { return l == line; })
        && std::ranges::all_of(cells, [&](int c) { return c == cell; });
}

// Each parameter moved from its default reaches the controls; a new processor's are the arpeggio object's own
void intoControls(TestChecks& check, const juce::File& rom) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    check(p.currentControls().arp == XtArpSettings {}, "a new processor's arpeggio should be the object's defaults");
    check(descriptorsPattern(p.arpPattern()), "and its pattern the descriptors' defaults");
    check(xtArpPatternSilent(p.arpPattern()), "which plays nothing");
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
bool arpPatch(const juce::File& rom, XtPatchFile& patch, std::vector<uint8_t>& bytes) {
    XtDiskImage disk;
    if (!disk.open(rom.getSiblingFile("V-SynthXTDiskImage.bin.0.bin").getFullPathName().toStdString())) return false;
    for (const auto& e : disk.list("Internal.prj/Patch")) {
        bytes = disk.read(e);
        if (!xtPatchRead(bytes.data(), bytes.size(), patch)) continue;
        const XtArpSettings s = xtArpSettingsOf(patch.store);
        if (s.on && !xtArpPatternSilent(xtArpPatternOf(patch.store)) && s.motif != 0 && s.grid != 3) return true;
    }
    return false;
}

// A session's state as one saved before v190: no pattern in it, and none of the arpeggio's parameters
juce::MemoryBlock beforeArp(const juce::MemoryBlock& state) {
    std::unique_ptr<juce::XmlElement> old(VSynthEmuProcessor::getXmlFromBinary(state.getData(), (int)state.getSize()));
    juce::MemoryBlock out;
    if (!old) return out;
    old->removeAttribute("arpPattern");
    for (auto* e = old->getFirstChildElement(); e != nullptr;) {
        auto* next = e->getNextElement();
        if (e->getStringAttribute("id").startsWith("arp")) old->removeChildElement(e, true);
        e = next;
    }
    VSynthEmuProcessor::copyXmlToBinary(*old, out);
    return out;
}

// Through loadPatch, then a session of it, and one from before v190
void patchAndSession(TestChecks& check, const juce::File& rom, const juce::File& work) {
    XtPatchFile patch;
    std::vector<uint8_t> bytes;
    if (!arpPatch(rom, patch, bytes)) {
        check(false, "a patch of the image's with the arpeggio on, beside the ROM");
        return;
    }
    const juce::File file = work.getChildFile("Patch/arp.p00");
    file.getParentDirectory().createDirectory();
    file.replaceWithData(bytes.data(), bytes.size());
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    check(p.loadPatch(file), "the patch should load");
    const std::string name = patch.name + ": ";
    check(p.currentControls().arp == xtArpSettingsOf(patch.store), name + "its arpeggio's settings on the parameters");
    check(p.arpPattern() == xtArpPatternOf(patch.store), name + "its pattern");
    check(!p.getPatchNote().contains("arpeggio"), name + "not said to be missing: " + p.getPatchNote().toStdString());

    juce::MemoryBlock state;
    p.getStateInformation(state);
    VSynthEmuProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    check(q.currentControls().arp == p.currentControls().arp, "a session's arpeggio settings should come back");
    check(q.arpPattern() == p.arpPattern(), "and its pattern");
    const juce::MemoryBlock before = beforeArp(state);
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

// The analog saw, UP on quarters at 120 BPM, a line of velocity 100: a note each quarter, as XtSynth plays it
void throughProcessor(TestChecks& check, const juce::File& rom, bool on) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    const Setting set[] = { { "oscType", 0 }, { "ampAttack", 0 }, { "ampRelease", 0 },
                            { "arpSwitch", on ? 1.0f : 0.0f }, { "arpGrid", 0 }, { "arpDuration", 25 } };
    for (const auto& [id, v] : set) setParam(p, id, v);
    XtArpPattern pattern = xtArpDefaultPattern();
    for (auto& step : pattern.cell) step[0] = 101;
    p.setArpPattern(pattern);
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
    return msmPluginTest(rom, work) | arp;   // the MSM's (v193)
}

// Every parameter as a host sees it, one a line
int paramList(const juce::File&, const juce::File&) {
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
    const bool list = argc > 2 && std::strcmp(argv[2], "params") == 0;
    return testMain(argc, argv, "arpplugin", list ? paramList : arpPluginTest);
}
