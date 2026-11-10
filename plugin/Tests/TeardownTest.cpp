// A processor closed while its wave is encoding (R1 of .claude/claude-packs-review.md).  A host does this when it drops
// an instance it has just restored, or when a project is closed straight after a wave is loaded.  The encode runs on
// the processor's pool thread, mostly in the interpreter; a 30 s BACKING encode takes about 11 s (measured 2026-11-09),
// past the 10 s the destructor used to wait, after which the job wrote into members already destroyed.  On Auto the
// job first runs the engine's content classifier, which no firmware stop reached (v185): closed in it, a processor
// waited for it to end, past those 10 s on a long wave.
// Run: VSynthEmuTeardownTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestControls.h"
#include "TestMain.h"
#include "TestWave.h"
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <numbers>
#include <thread>

namespace {

// The Encode Type's choice index: 0 Auto, 1 SOLO, 2 BACKING, 3 ENSEMBLE, 4 LITE
constexpr float kAuto = 0.0f, kBacking = 2.0f;
constexpr int kAutoSeconds = 360;   // a wave Auto's classifier takes more than 10 s on (measured, v185)

std::unique_ptr<VSynthEmuProcessor> encodingAs(const juce::File& rom, const juce::File& wave, float type) {
    auto p = std::make_unique<VSynthEmuProcessor>();
    if (!p->setRomFile(rom)) return nullptr;
    setParam(*p, "encodeType", type);
    p->loadFile(wave);
    return p;
}

// A processor encoding `wave` as `type`, closed 1 s in: how long the close took, in ms; negative if it couldn't start
double closedMidEncode(const juce::File& rom, const juce::File& wave, float type, TestChecks& checks) {
    auto p = encodingAs(rom, wave, type);
    if (!p) return -1.0;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    checks(p->getStatusText() == "Encoding...", wave.getFileName().toStdString() + " should still be encoding at 1 s: "
                                                    + p->getStatusText().toStdString());
    const auto t0 = std::chrono::steady_clock::now();
    p.reset();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

int teardownTest(const juce::File& rom, const juce::File& work) {
    TestChecks checks { "teardown" };
    const auto check = [&checks](bool ok, const juce::String& what) { checks(ok, what.toStdString()); };
    work.createDirectory();
    const juce::File longWave = writeTestWave(work, 30), shortWave = writeTestWave(work, 2);
    const juce::File autoWave = writeTestWave(work, kAutoSeconds);
    if (longWave == juce::File() || shortWave == juce::File() || autoWave == juce::File()) {
        std::fprintf(stderr, "teardown: no test waves\n");
        return 1;
    }

    const double ms = closedMidEncode(rom, longWave, kBacking, checks);
    if (ms < 0) { std::fprintf(stderr, "teardown: ROM rejected\n"); return 1; }
    check(ms < 2000.0, "closing a processor mid-encode should take under 2 s, not " + juce::String(ms, 0) + " ms");

    // Under 0.5 s: the classifier stops within a frame, and no encode follows (one would take about 1 s on this wave)
    const double autoMs = closedMidEncode(rom, autoWave, kAuto, checks);
    check(autoMs >= 0 && autoMs < 500.0, "closing a processor in Auto's classifier should take under 0.5 s, not "
                                             + juce::String(autoMs, 0) + " ms");

    // Nothing of those encodes is left to stop the next: a new processor encodes as before.
    auto q = encodingAs(rom, shortWave, kBacking);
    for (int i = 0; q && i < 600 && !q->getStatusText().startsWith("Encoded"); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const juce::String status = q ? q->getStatusText() : juce::String("no processor");
    check(status == "Encoded as BACKING", "the next processor should encode: " + status);
    q.reset();
    const int held = checks.count - checks.failed;
    std::printf("teardown: %d/%d checks hold; closed mid-encode in %.0f ms, in Auto's classifier in %.0f ms\n", held,
                checks.count, ms, autoMs);
    return checks.failed ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) { return testMain(argc, argv, "teardown", teardownTest); }
