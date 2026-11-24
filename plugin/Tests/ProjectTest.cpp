// A project's own files through the processor (v600 on).  v600: a project's link to a preset wave (.w00).  A patch of
// the image's whose OSC 1 plays wave N, in a project whose slot N links to another preset wave, M: the patch's load
// follows the link to M, not to N (in Internal.prj slot n links to preset n, so a reader that took the slot for the
// preset would pass there), the OSC holds the link's file and M's own settings, and a link loads into an OSC by itself
// too.  A link past the library loads nothing and the patch's note says why.  The waves expected are read apart from the
// plugin's reader: the disk image opened here, its library built, and each stream read from its bytes.
// Run: VSynthEmuProjectTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "TestPatches.h"
#include "../Source/PluginPatchFiles.h"
#include "../Source/PluginProcessor.h"
#include "../Source/XtPatch.h"
#include "../Source/XtPresetWaves.h"
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kPastLibrary = 999;
constexpr const char* kDiskImageName = "V-SynthXTDiskImage.bin.0.bin";

// A link file's bytes, as the XT writes one: "link", 4, the number; "name", 16, the name padded with spaces
std::vector<uint8_t> linkFile(int number, const std::string& name) {
    std::vector<uint8_t> f { 'l', 'i', 'n', 'k', 4, 0, 0, 0 };
    for (int b = 0; b < 4; ++b) f.push_back(static_cast<uint8_t>(number >> (8 * b)));
    f.insert(f.end(), { 'n', 'a', 'm', 'e', 16, 0, 0, 0 });
    std::string padded = name.substr(0, 16);
    padded.resize(16, ' ');
    f.insert(f.end(), padded.begin(), padded.end());
    return f;
}

// The preset waves, read apart from the plugin's reader
struct Presets {
    XtDiskImage disk;
    XtPresetWaves waves;
    std::optional<XtWaveFile> read(int number) const {
        const XtPresetWave* w = waves.wave(number);
        XtWaveFile out;
        if (!w || !xtWaveRead(&disk.bytes()[w->at], w->bytes, out)) return std::nullopt;
        return out;
    }
};

// The OSC's controls hold this wave's own settings: putting them on again changes nothing
bool sameSettings(const XtSynth::OscControls& o, const XtWaveFile& w) {
    XtSynth::OscControls with = o;
    xtWaveOntoControls(w, with);
    return with.rootKey == o.rootKey && with.origTempo == o.origTempo && with.loopStart == o.loopStart
           && with.loopEnd == o.loopEnd && with.loopMode == o.loopMode;
}

// The first patch whose OSC 1 plays a preset wave, and a preset wave other than its own whose settings aren't its own's
struct Case {
    ImagePatch patch;
    int own = 0, other = 0;
    XtWaveFile otherWave;
};
std::optional<Case> caseOf(const juce::File& rom, const Presets& presets) {
    for (ImagePatch& m : imagePatches(rom)) {
        const int own = xtPatchWaveNumber(m.store, 0, 0);
        const auto ownWave = presets.read(own);
        if (!ownWave) continue;
        XtSynth::OscControls a;
        xtWaveOntoControls(*ownWave, a);
        for (int other = presets.waves.count(); other >= 1; --other) {
            const auto o = presets.read(other);
            if (other == own || !o) continue;
            XtSynth::OscControls b;
            xtWaveOntoControls(*o, b);
            if (a.rootKey != b.rootKey || a.origTempo != b.origTempo) return Case { m, own, other, *o };
        }
    }
    return std::nullopt;
}

juce::File waveFile(const juce::File& project, int slot, const std::string& name) {
    return project.getChildFile("Wave").getChildFile(juce::String(slot).paddedLeft('0', 3) + " " + name + ".w00");
}

int run(const juce::File& rom, const juce::File& work) {
    TestChecks check { "VSynthEmuProjectTest" };
    VSynthEmuProcessor q;   // the firmware in, for the patches' and the waves' reads; later a link into its OSC 2
    check(q.setRomFile(rom), "the ROM should open");
    Presets presets;
    const bool image = presets.disk.open(rom.getSiblingFile(kDiskImageName).getFullPathName().toStdString());
    check(image && presets.waves.build(presets.disk), "the disk image beside the ROM should hold the preset waves");
    const std::optional<Case> c = image ? caseOf(rom, presets) : std::nullopt;
    check(c.has_value(), "a patch of the image's whose OSC 1 plays a preset wave");
    if (!c) return check.summary();

    // Slot `own` of a project links to preset `other`
    const juce::File project = work.getChildFile("Linked.prj");
    const std::string otherName = presets.waves.wave(c->other)->name;
    const juce::File link = waveFile(project, c->own, otherName);
    writeTestFile(link, linkFile(c->other, otherName));
    check(isVSynthWave(link), "a link file should be taken for a V-Synth wave");
    auto p = loadedWith(check, rom, c->patch, project.getChildFile("Patch").getChildFile("001 Linked.p00"));
    const XtSynth::Controls now = p->currentControls();
    check(p->hasFileLoaded(0) && p->getLoadedFileName(0) == link.getFileName(), "OSC 1 should hold the link's file");
    check(sameSettings(now, c->otherWave), "OSC 1 should have the linked wave's own settings, preset "
                                               + std::to_string(c->other) + "'s, not slot " + std::to_string(c->own)
                                               + "'s preset's");

    // A link by itself into OSC 2
    q.loadFile(link, 1);
    check(q.hasFileLoaded(1) && q.getLoadedFileName(1) == link.getFileName(),
          "a link should load into an OSC by itself");

    // A link past the library: nothing for OSC 1, and the note says why
    const juce::File past = work.getChildFile("Past.prj");
    writeTestFile(waveFile(past, c->own, "Nowhere"), linkFile(kPastLibrary, "Nowhere"));
    auto r = loadedWith(check, rom, c->patch, past.getChildFile("Patch").getChildFile("001 Past.p00"));
    check(!r->hasFileLoaded(0), "a link past the library should load nothing");
    check(r->getPatchNote().contains("no wave " + juce::String(c->own) + ": no such preset wave"),
          "the patch's note should say why its wave is missing: " + r->getPatchNote().toStdString());
    return check.summary("patch " + c->patch.name + ", slot " + std::to_string(c->own) + " linked to preset "
                         + std::to_string(c->other));
}

} // namespace

int main(int argc, char** argv) { return testMain(argc, argv, "VSynthEmuProjectTest", run); }
