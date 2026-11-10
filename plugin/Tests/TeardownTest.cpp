// A processor closed while its wave is encoding (R1 of .claude/claude-packs-review.md).  A host does this when it drops
// an instance it has just restored, or when a project is closed straight after a wave is loaded.  The encode runs on
// the processor's pool thread, mostly in the interpreter; a 30 s BACKING encode takes about 11 s (measured 2026-11-09),
// past the 10 s the destructor used to wait, after which the job wrote into members already destroyed.
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

constexpr float kBacking = 2.0f;   // the Encode Type's choice index: 0 Auto, 1 SOLO, 2 BACKING, 3 ENSEMBLE, 4 LITE

std::unique_ptr<VSynthEmuProcessor> backingOn(const juce::File& rom, const juce::File& wave) {
    auto p = std::make_unique<VSynthEmuProcessor>();
    if (!p->setRomFile(rom)) return nullptr;
    setParam(*p, "encodeType", kBacking);
    p->loadFile(wave);
    return p;
}

int teardownTest(const juce::File& rom, const juce::File& work) {
    TestChecks checks { "teardown" };
    const auto check = [&checks](bool ok, const juce::String& what) { checks(ok, what.toStdString()); };
    work.createDirectory();
    const juce::File longWave = writeTestWave(work, 30), shortWave = writeTestWave(work, 2);
    if (longWave == juce::File() || shortWave == juce::File()) {
        std::fprintf(stderr, "teardown: no test waves\n");
        return 1;
    }

    auto p = backingOn(rom, longWave);
    if (!p) { std::fprintf(stderr, "teardown: ROM rejected\n"); return 1; }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    check(p->getStatusText() == "Encoding...", "the 30 s wave should still be encoding at 1 s: " + p->getStatusText());
    const auto t0 = std::chrono::steady_clock::now();
    p.reset();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    check(ms < 2000.0, "closing a processor mid-encode should take under 2 s, not " + juce::String(ms, 0) + " ms");

    // Nothing of that encode is left to stop the next: a new processor encodes as before.
    auto q = backingOn(rom, shortWave);
    for (int i = 0; q && i < 600 && !q->getStatusText().startsWith("Encoded"); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const juce::String status = q ? q->getStatusText() : juce::String("no processor");
    check(status == "Encoded as BACKING", "the next processor should encode: " + status);
    q.reset();
    const int held = checks.count - checks.failed;
    std::printf("teardown: %d/%d checks hold; closed mid-encode in %.0f ms\n", held, checks.count, ms);
    return checks.failed ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) { return testMain(argc, argv, "teardown", teardownTest); }
