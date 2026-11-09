// A processor closed while its wave is encoding (R1 of .claude/claude-packs-review.md).  A host does this when it drops
// an instance it has just restored, or when a project is closed straight after a wave is loaded.  The encode runs on
// the processor's pool thread, mostly in the interpreter; a 30 s BACKING encode takes about 11 s (measured 2026-11-09),
// past the 10 s the destructor used to wait, after which the job wrote into members already destroyed.
// Run: VSynthEmuTeardownTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestAppDir.h"
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <numbers>
#include <thread>

namespace {

constexpr int kRate = 44100;
constexpr float kBacking = 2.0f;   // the Encode Type's choice index: 0 Auto, 1 SOLO, 2 BACKING, 3 ENSEMBLE, 4 LITE

// `seconds` of a line of notes, a semitone up every half second, with a little noise: something the encoders work at.
juce::File writeWave(const juce::File& folder, int seconds) {
    const int frames = seconds * kRate;
    juce::AudioBuffer<float> audio(1, frames);
    uint32_t noise = 1;
    for (int i = 0; i < frames; ++i) {
        const double t = (double)i / kRate, f = 110.0 * std::pow(2.0, (double)((i / (kRate / 2)) % 12) / 12.0);
        const double env = 0.6 + 0.4 * std::exp(-8.0 * std::fmod(t, 0.5));
        noise = noise * 1664525u + 1013904223u;
        const double n = (double)(noise >> 8) / 16777216.0 - 0.5;
        audio.setSample(0, i, (float)(0.4 * std::sin(2.0 * std::numbers::pi * f * t) * env + 0.01 * n));
    }
    const juce::File file = folder.getChildFile("teardown_" + juce::String(seconds) + "s.wav");
    file.deleteFile();
    juce::WavAudioFormat wav;
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    if (!stream->openedOk()) return {};
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), kRate, 1, 16, {}, 0));
    if (!writer) return {};
    stream.release();   // the writer owns it now
    return writer->writeFromAudioSampleBuffer(audio, 0, frames) ? file : juce::File();
}

std::unique_ptr<VSynthEmuProcessor> backingOn(const juce::File& rom, const juce::File& wave) {
    auto p = std::make_unique<VSynthEmuProcessor>();
    if (!p->setRomFile(rom)) return nullptr;
    auto* type = p->apvts.getParameter("encodeType");
    type->setValueNotifyingHost(type->convertTo0to1(kBacking));
    p->loadFile(wave);
    return p;
}

int teardownTest(const juce::File& rom, const juce::File& work) {
    int checks = 0, failed = 0;
    const auto check = [&](bool ok, const juce::String& what) {
        ++checks;
        if (!ok) { ++failed; std::fprintf(stderr, "teardown: %s\n", what.toRawUTF8()); }
    };
    work.createDirectory();
    const juce::File longWave = writeWave(work, 30), shortWave = writeWave(work, 2);
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
    std::printf("teardown: %d/%d checks hold; closed mid-encode in %.0f ms\n", checks - failed, checks, ms);
    return failed ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s rom.2.bin\n", argv[0]); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    TestAppDir appDir;
    const juce::File rom = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    return teardownTest(rom, appDir.dir.getChildFile("teardown"));
}
