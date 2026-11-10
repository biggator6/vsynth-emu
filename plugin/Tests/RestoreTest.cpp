// A project that reopens without its parts (R6 of .claude/claude-packs-review.md): what a session can't bring back is
// said, not dropped.  A wave whose file has gone; a patch's zones whose waves aren't in its folder, saved when nothing was
// missing; and effects whose driver won't start (here, the copy of the ROM they were given, taken away before the Effects
// switch goes on).
// Run: VSynthEmuRestoreTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestAppDir.h"
#include "TestChecks.h"
#include "TestControls.h"
#include "TestWave.h"
#include "../Source/PluginProcessor.h"
#include "../Source/XtFiles.h"
#include "../Source/XtPatch.h"
#include <algorithm>
#include <memory>
#include <ranges>
#include <vector>

namespace {

// A session's state, from a processor that `does` something first
template <typename Does>
juce::MemoryBlock saved(const juce::File& rom, Does does) {
    juce::MemoryBlock state;
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    does(p);
    p.getStateInformation(state);
    return state;
}

void waveGone(TestChecks& check, const juce::File& rom, const juce::File& work) {
    const juce::File wave = writeTestWave(work, 2);
    const auto state = saved(rom, [&](VSynthEmuProcessor& p) { p.loadFile(wave); });
    wave.deleteFile();
    VSynthEmuProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    const std::string said = ("note \"" + q.getPatchNote() + "\", status \"" + q.getStatusText() + "\"").toStdString();
    check(q.getPatchNote().contains(wave.getFileName()),
          "a wave whose file has gone should be in what's not here: " + said);
    check(q.getStatusText().contains(wave.getFileName()), "and in the status line: " + said);
}

// Does a zone past the first play a wave, by the patch's own OSC Type and Wave Number, as loadZones looks for one?
bool zonesPlayWaves(const std::vector<uint8_t>& store) {
    auto set = std::make_unique<XtSynth::Zones>();
    xtPatchZoneSet(store, XtSynth::Controls {}, *set);
    const auto plays = [&](int zone) {
        const XtSynth::OscControls& osc1 = set->ctl[zone];
        return (osc1.oscType == 1 && xtPatchWaveNumber(store, zone, 0) > 0)
            || (set->ctl[zone].osc2.oscType == 1 && xtPatchWaveNumber(store, zone, 1) > 0);
    };
    return std::ranges::any_of(std::views::iota(1, xtPatchZones(store)), plays);
}

void zonesWithoutWaves(TestChecks& check, const juce::File& rom, const juce::File& work) {
    XtDiskImage disk;
    if (!disk.open(rom.getSiblingFile("V-SynthXTDiskImage.bin.0.bin").getFullPathName().toStdString())) {
        check(false, "no disk image beside the ROM");
        return;
    }
    const auto entries = disk.list("Internal.prj/Patch");
    const auto chosen = std::ranges::find_if(entries, [&](const XtDiskImage::Entry& e) {
        const auto bytes = disk.read(e);
        XtPatchFile p;
        return xtPatchRead(bytes.data(), bytes.size(), p) && zonesPlayWaves(p.store);
    });
    if (chosen == entries.end()) {
        check(false, "a patch of the image's whose zones past the first play waves");
        return;
    }
    const auto bytes = disk.read(*chosen);
    const juce::File patch = work.getChildFile("Project/Patch/zones.p00");   // and no Wave folder beside it
    patch.getParentDirectory().createDirectory();
    patch.replaceWithData(bytes.data(), bytes.size());
    // Saved as if its waves had been there: nothing said in the session
    const auto state = saved(rom, [&](VSynthEmuProcessor& p) {
        p.loadPatch(patch);
        p.apvts.state.setProperty("patchNote", juce::String(), nullptr);
    });
    VSynthEmuProcessor q;
    q.setStateInformation(state.getData(), (int)state.getSize());
    const std::string note = q.getPatchNote().toStdString();
    check(q.patchZones() > 1, "the patch's zones should come back");
    check(q.getPatchNote().contains("no wave"), "zones whose waves aren't there should say so: \"" + note + "\"");
}

void effectsWithoutRom(TestChecks& check, const juce::File& rom, const juce::File& work) {
    const juce::File copy = work.getChildFile("rom copy.bin");
    if (!rom.copyFileTo(copy)) {
        check(false, "can't copy the ROM");
        return;
    }
    VSynthEmuProcessor p;
    p.fxManualTicks();
    const bool opened = p.setRomFile(copy);
    copy.deleteFile();
    setParam(p, "fxOn", 1.0f);
    p.fxTick();
    check(opened && p.getStatusText().startsWith("Effects"),
          "effects whose driver won't start should say so: \"" + p.getStatusText().toStdString() + "\"");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s rom.2.bin\n", argc > 0 ? argv[0] : "");
        return 1;
    }
    juce::ScopedJuceInitialiser_GUI init;
    TestAppDir appDir;
    const juce::File rom = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    const juce::File work = appDir.dir.getChildFile("restore");
    work.createDirectory();
    TestChecks check { "restore" };
    waveGone(check, rom, work);
    zonesWithoutWaves(check, rom, work);
    effectsWithoutRom(check, rom, work);   // last: it leaves the process's tables read from a file that's gone
    return check.summary();
}
