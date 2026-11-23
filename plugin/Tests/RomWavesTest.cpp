// VSynthEmuRomTest's checks of the VC-2's waves at the plugin's start (v504; Vc2WavesInUse.h).  rom.4 is found beside
// rom.2 and the disk image beside rom.4, as the firmware update's unpacker leaves them:
// - the start takes the disk image and loads the waves on a thread of its own: the load ends within a minute, its
//   status names the disk image beside rom.4, and what it loaded is rom.4's own load of the same files byte for byte
//   (the store's words, the pool's words and memory, the sets' counts, tables, buffers and data);
// - a fresh run that ends as the load runs ends at once, the load stopped: more than a second sooner than a fresh run
//   that waits for the load, which takes about 2 s.
// With rom.4 alone in an app folder and no disk image anywhere it's sought, none is taken (RomTest.cpp's vc2Alone).
#include "RomWavesTest.h"
#include "TestMain.h"
#include "../Source/PluginProcessor.h"
#include "../Source/Vc2WavesInUse.h"
#include "../Source/XtEncoded.h"
#include <cstdio>
#include <cstring>
#include <tuple>

namespace {
constexpr double kLoadMost = 60;     // seconds: the disk image's load takes about 2
constexpr double kStopSooner = 1;    // seconds a run that ends as the load runs is sooner than one that waits for it
const juce::String kQuits = "--waves-quit", kWaits = "--waves-wait";

// Two loads' waves byte for byte: what differs first, empty when nothing does
std::string wavesDifference(const Vc2LoadedWaves& a, const Vc2LoadedWaves& b) {
    const auto same = [](const void* x, const void* y, size_t n) { return std::memcmp(x, y, n) == 0; };
    const std::tuple<const char*, bool> parts[] = {
        { "the store's words", same(&a.store.words, &b.store.words, sizeof a.store.words) },
        { "the pool's words", same(&a.pool.words, &b.pool.words, sizeof a.pool.words) },
        { "the pool's memory", a.pool.memory == b.pool.memory },
        { "the sets' counts", same(&a.sets.counts, &b.sets.counts, sizeof a.sets.counts) },
        { "the sets' tables", same(&a.sets.tables, &b.sets.tables, sizeof a.sets.tables) },
        { "the sets' buffers", same(&a.sets.buffers, &b.sets.buffers, sizeof a.sets.buffers) },
        { "the sets' data", a.sets.data.velocity == b.sets.data.velocity && a.sets.data.key == b.sets.data.key },
    };
    for (const auto& [what, ok] : parts)
        if (!ok) return what;
    return "";
}

// A fresh run of this program as `mode`, given rom.2 at `rom` first (VSE_XT_ROM: what the earlier checks kept names a
// copy with no rom.4 beside it): how long it took, in seconds, and what it said
double runFor(const juce::String& mode, const juce::File& rom, juce::String& said) {
    setenv("VSE_XT_ROM", rom.getFullPathName().toRawUTF8(), 1);
    const double from = juce::Time::getMillisecondCounterHiRes();
    said = thisProgramSays({ rom.getFullPathName(), mode });
    const double took = (juce::Time::getMillisecondCounterHiRes() - from) / 1000;
    unsetenv("VSE_XT_ROM");
    return took;
}

std::string quoted(const juce::String& said) { return "\"" + said.trim().toStdString() + "\""; }
} // namespace

void wavesAtStart(TestChecks& check, const juce::File& rom) {
    const juce::File disk = rom.getSiblingFile("V-SynthXTDiskImage.bin.0.bin");
    const std::string there = disk.getFullPathName().toStdString();
    check(disk.existsAsFile(), "the disk image should be beside rom.4: " + there);
    check(vc2WavesTaken(), "the start should take the disk image beside rom.4: " + vc2WavesStatus());
    check(vc2WavesWait(kLoadMost), "the waves' load should end within a minute: " + vc2WavesStatus());
    const Vc2LoadedWaves* loaded = vc2Waves();
    check(loaded && vc2WavesStatus() == "loaded from " + there, "the waves should load: \"" + vc2WavesStatus() + "\"");
    XtDiskImage image;
    check(image.open(there), "the test can't read the disk image");
    const Vc2DiskBootFiles files(image);
    Vc2LoadedWaves direct;
    std::string why;
    check(vc2LoadWaves(xtSynthVc2RomPath(), files, direct, &why), "rom.4's own load of the same files: " + why);
    const std::string differs = loaded ? wavesDifference(*loaded, direct) : "none loaded";
    check(differs.empty(), "the start's waves should be rom.4's own load of the same files: " + differs);
    juce::String quit, waited;
    const double quitting = runFor(kQuits, rom, quit), waiting = runFor(kWaits, rom, waited);
    check(waited.contains("waves loaded"), "a fresh run should load the waves: " + quoted(waited));
    check(quit.contains("waves taken") && quitting + kStopSooner < waiting,
          "a run that ends as the waves load should end at once: " + std::to_string(quitting) + " s, against "
              + std::to_string(waiting) + " s waiting; " + quoted(quit));
}

int wavesChild(const juce::String& mode) {
    if (mode != kQuits && mode != kWaits) return -1;
    juce::ScopedJuceInitialiser_GUI init;
    const VSynthEmuProcessor p;
    if (mode == kWaits) vc2WavesWait(kLoadMost);
    std::printf("waves %s\n", vc2Waves() ? "loaded" : vc2WavesTaken() ? "taken" : "not taken");
    return 0;
}
