// A firmware routine that never returns, on each way the plugin runs one (.claude/fix-plan.md, section 9; v175): a
// patch's load and a .vpw's read on the message thread, and a wave's encode on the pool.  The test's image is a copy of
// the ROM with three routines made to loop (`bra .`, a nop in its slot): the patch store (0x8947dae0), an RDAC block's
// decode (0x89529570) and the attack detection every encode starts with at Encode Depth 64 (0x895164c0).  The tables
// the processor loads use none of them.  Each way must give up within its bound and say so, where every call used to
// be let run 20e9 steps, over a minute; a SOLO wave then falls back to the port's encoder and plays.  A watchdog
// ends the run if a way still holds its thread at kDeadline.
// Run: VSynthEmuStepTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "TestNote.h"
#include "TestWave.h"
#include "../Source/PluginProcessor.h"
#include "../Source/XtFiles.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t kRomBase = 0x89300000;                                 // where rom.2 maps
constexpr uint32_t kLooped[] = { 0x8947dae0, 0x89529570, 0x895164c0 };    // patch store, RDAC block, detection
constexpr uint8_t kLoop[] = { 0xfe, 0xaf, 0x09, 0x00 };                   // bra . (to itself), nop in its slot
constexpr auto kDeadline = std::chrono::seconds(30);
constexpr double kMessageBound = 1.0;      // seconds a looping patch or .vpw may hold the message thread
constexpr double kEncodeBound = 10.0;      // and an encode of the 1 s test wave, from load to its status
constexpr int kBlock = 512, kBlocks = 64;  // 0.68 s at 48 kHz
constexpr float kSolo = 1.0f, kBacking = 2.0f;
constexpr float kHeard = 0.001f;

// Ends the run, saying what held, if the scope is still open at kDeadline: a way no step limit stopped
class Watchdog {
public:
    explicit Watchdog(const char* what)
        : thread_([this, what] {
              std::unique_lock lock(m_);
              if (cv_.wait_for(lock, kDeadline, [this] { return done_; })) return;
              std::fprintf(stderr, "steps: %s still holds its thread after %lld s\n", what,
                           (long long)kDeadline.count());
              std::_Exit(1);
          }) {}
    ~Watchdog() {
        {
            const std::lock_guard lock(m_);
            done_ = true;
        }
        cv_.notify_one();
        thread_.join();
    }
    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;

private:
    std::mutex m_;
    std::condition_variable cv_;
    bool done_ = false;
    std::thread thread_;   // last, so it starts with the rest made
};

// Seconds `fn` takes, under a watchdog
template <class F>
double timed(const char* what, F&& fn) {
    const Watchdog dog(what);
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// The ROM with the three routines looping, in the work folder
juce::File loopingRom(const juce::File& rom, const juce::File& work) {
    juce::MemoryBlock b;
    if (!rom.loadFileAsData(b)) return {};
    for (const uint32_t a : kLooped) {
        const size_t at = a - kRomBase;
        if (at + sizeof kLoop > b.getSize()) return {};
        std::memcpy(static_cast<uint8_t*>(b.getData()) + at, kLoop, sizeof kLoop);
    }
    const juce::File out = work.getChildFile("looping.rom.2.bin");
    return out.replaceWithData(b.getData(), b.getSize()) ? out : juce::File();
}

// A patch and an RDAC preset wave (its vpfl says so at +40) from the firmware's disk image beside the ROM, as files
// of their own in the work folder.  The wave runs to the next one's start: its reader stops at its data chunk's end.
bool filesFromImage(const juce::File& rom, const juce::File& work, juce::File& patch, juce::File& wave) {
    XtDiskImage disk;
    if (!disk.open(rom.getSiblingFile("V-SynthXTDiskImage.bin.0.bin").getFullPathName().toStdString())) return false;
    const auto patches = disk.list("Internal.prj/Patch");
    const auto file = std::ranges::find_if(patches, [](const XtDiskImage::Entry& e) { return !e.isDir; });
    const std::vector<size_t> waves = disk.presetWaves();
    const std::vector<uint8_t>& b = disk.bytes();
    const auto rdac = std::ranges::adjacent_find(waves, [&b](size_t at, size_t) {
        return at + 52 <= b.size() && !std::memcmp(&b[at + 48], "RDAC", 4);
    });
    if (file == patches.end() || rdac == waves.end()) return false;
    const std::vector<uint8_t> p = disk.read(*file);
    patch = work.getChildFile("patch.p00");
    wave = work.getChildFile("preset.vpw");
    return patch.replaceWithData(p.data(), p.size()) && wave.replaceWithData(&b[rdac[0]], rdac[1] - rdac[0]);
}

std::string seconds(double s) {
    char t[32];
    std::snprintf(t, sizeof t, "%.2f s", s);
    return t;
}

int stepTest(const juce::File& rom, const juce::File& work) {
    TestChecks check { "steps" };
    juce::File patch, vpw;
    const juce::File looping = loopingRom(rom, work), wav = writeTestWave(work, 1);
    if (looping == juce::File() || wav == juce::File() || !filesFromImage(rom, work, patch, vpw)) {
        std::fprintf(stderr, "steps: no image to loop, test wave, or disk image beside the ROM\n");
        return 1;
    }
    VSynthEmuProcessor p;
    check(p.setRomFile(looping), "the image should be taken: the tables use none of the routines that loop");

    bool loaded = true;
    const double tPatch = timed("a patch's load", [&] { loaded = p.loadPatch(patch); });
    check(!loaded && p.getStatusText().startsWith(patch.getFileName() + ": "),
          "a patch whose store loops should fail, saying so: " + p.getStatusText().toStdString());
    check(tPatch < kMessageBound, "a patch's looping store held the message thread " + seconds(tPatch));

    const double tWave = timed("a .vpw's read", [&] { p.loadFile(vpw); });
    check(p.getStatusText().startsWith(vpw.getFileName() + ": "),
          "a .vpw whose decode loops should fail, saying so: " + p.getStatusText().toStdString());
    check(tWave < kMessageBound, "a .vpw's looping decode held the message thread " + seconds(tWave));

    juce::String status;
    const double tSolo = timed("a SOLO encode", [&] { status = encodeWave(p, wav, kSolo); });
    check(status.startsWith("Encoded"), "a SOLO wave should fall back to the port's encoder: " + status.toStdString());
    check(tSolo < kEncodeBound, "a SOLO encode whose detection loops took " + seconds(tSolo));
    float peak = 0.0f;
    for (const float s : playNote(p, kBlock, kBlocks, 2)) peak = std::max(peak, std::fabs(s));
    check(peak > kHeard, "the SOLO wave the port encoded should play");

    const double tBacking = timed("a BACKING encode", [&] { status = encodeWave(p, wav, kBacking); });
    check(status.startsWith("Encode failed"), "a BACKING encode whose detection loops should fail, saying so: "
                                                  + status.toStdString());
    check(tBacking < kEncodeBound, "a BACKING encode whose detection loops took " + seconds(tBacking));
    return check.summary("a looping patch gave up in " + seconds(tPatch) + ", a .vpw in " + seconds(tWave)
                         + ", encodes in " + seconds(tSolo) + " and " + seconds(tBacking));
}

} // namespace

int main(int argc, char** argv) {
    return testMain(argc, argv, "steps", stepTest);
}
