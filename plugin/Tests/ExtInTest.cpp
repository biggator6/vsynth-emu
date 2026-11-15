// VSynthEmuExtInTest (v207): the outside input from the plugin's input bus to the EXT IN OSC.  The processor against
// XtSynth is ExtInPluginTest.cpp's; here, the feed alone (XtHostInput):
// - a host sample's word: x × 2^31 toward zero, the limits past ±1, silence for what isn't finite;
// - at 44.1 kHz each frame's word as it came, latency() frames late, whatever the host's blocks;
// - at nine rates from 8 to 192 kHz, XtSynth taking its blocks while a host pushes blocks of every size up to most(),
//   as the processor does: no block short, nothing overrun;
// - a 1 and a 10 kHz sine at 48 and 96 kHz as the same sine at 44.1 kHz, on time, within 10^-4 of full scale;
// - a 30 kHz tone at 96 kHz, past the chip's Nyquist, 40 dB down at least, where the kernel for the other way passes it.
// Run: VSynthEmuExtInTest <rom.2.bin>.  Exit 0 if every check holds.
#include "ExtInTest.h"
#include "TestMain.h"
#include "../Source/XtEncoded.h"
#include "../Source/XtHostInput.h"
#include "../Source/XtSynth.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace {

constexpr double kChipRate = 44100.0;
constexpr int kBlock = 32;               // what XtSynth's busOut takes a block
constexpr int kMostBlock = 2048;         // the largest host block drawn but most() itself
constexpr long kStartup = 64;            // frames after the latency before the kernel has only the host's frames
constexpr double kRates[] = { 8000.0, 22050.0, 32000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };

int32_t wordOf24(int32_t sample24) { return (int32_t)((uint32_t)sample24 << 8); }

void wordOf(TestChecks& check) {
    struct Case {
        float x;
        int32_t word;
        const char* what;
    };
    const Case cases[] = { { 0.5f, 1 << 30, "a half" },
                           { -0.5f, -(1 << 30), "a half down" },
                           { 0x1p-23f, 256, "a 24-bit step" },
                           { -0x1p-31f, -1, "the last bit down" },
                           { 0x1.fffffep-1f, 2147483520, "the float below 1" },
                           { 1.0f, INT32_MAX, "full scale" },
                           { -1.0f, INT32_MIN, "full scale down" },
                           { 1.5f, INT32_MAX, "past it" },
                           { -2.0f, INT32_MIN, "past it down" },
                           { std::nanf(""), 0, "not a number" },
                           { INFINITY, 0, "infinity" } };
    for (const Case& c : cases) {
        const int32_t got = XtHostInput::wordOf(c.x);
        check(got == c.word, std::string("a host sample's word, ") + c.what + ": " + std::to_string(got));
    }
}

// The host's sizes of block, the same each run: 1, most(), and between
int blockOf(juce::Random& rng, int most) {
    const int between = 1 + rng.nextInt(std::min(most, kMostBlock));
    const int sizes[] = { 1, most, between, between };
    return sizes[rng.nextInt(4)];
}

// The host's frames through `in` in its blocks, each block's 44.1 kHz frames taken as they're made: the words, L then R
std::vector<int32_t> through(XtHostInput& in, const std::vector<float>& l, const std::vector<float>& r, int seed) {
    juce::Random rng(seed);
    std::vector<int32_t> got;
    for (size_t at = 0; at < l.size();) {
        const size_t n = std::min(l.size() - at, (size_t)blockOf(rng, in.most()));
        in.push(l.data() + at, r.data() + at, (int)n);
        at += n;
        while (in.ready() >= (uint32_t)kBlock) {
            const int32_t* b = in.block();
            got.insert(got.end(), b, b + 2 * kBlock);
        }
    }
    return got;
}

void direct(TestChecks& check) {
    constexpr long kFrames = 20000;
    std::vector<float> l(kFrames), r(kFrames);
    for (long i = 0; i < kFrames; ++i) {
        l[(size_t)i] = hostSample(i, 0);
        r[(size_t)i] = hostSample(i, 1);
    }
    XtHostInput in;
    in.prepare(kChipRate);
    const std::vector<int32_t> got = through(in, l, r, 207);
    long wrong = 0;
    for (long k = 0; k < (long)got.size() / 2; ++k)
        for (int side = 0; side < 2; ++side) {
            const long j = k - in.latency();
            wrong += got[(size_t)(2 * k + side)] != (j < 0 ? 0 : wordOf24(hostSample24(j, side)));
        }
    check(wrong == 0 && got.size() / 2 + 2 * kBlock >= (size_t)kFrames,
          "at 44.1 kHz each frame's word as it came, " + std::to_string(in.latency()) + " frames late: " +
              std::to_string(wrong) + " words of " + std::to_string(got.size()) + " not");
    check(in.latency() <= 2 * kBlock, "and that latency within two blocks: " + std::to_string(in.latency()));
}

void everyRate(TestChecks& check) {
    for (const double rate : kRates) {
        XtSynth synth;   // no voices: it renders silence, taking its blocks as the processor's does
        synth.prepare(rate);
        XtHostInput in;
        in.prepare(rate, synth.input);
        std::vector<float> outL((size_t)in.most()), outR((size_t)in.most());
        juce::Random rng((juce::int64)rate);
        for (long done = 0; done < (long)(2.0 * rate);) {
            const int n = blockOf(rng, in.most());
            in.push(nullptr, nullptr, n);
            synth.render(outL.data(), outR.data(), n);
            done += n;
        }
        check(in.shortBlocks() == 0 && in.overruns() == 0,
              "at " + std::to_string((int)rate) + " Hz in blocks up to " + std::to_string(in.most()) + ": " +
                  std::to_string(in.shortBlocks()) + " blocks short, " + std::to_string(in.overruns()) + " overruns");
    }
}

// A tone at the host's rate through the feed, against the same tone at 44.1 kHz on time (frame k − latency() at
// k / 44.1 kHz), after the startup: the most a frame is off it, and the RMS of what came, both as shares of full scale
struct ToneThrough {
    double off = 0.0, rms = 0.0;
};
ToneThrough tone(double rate, double hz, double amp) {
    const auto n = (size_t)rate;   // a second
    std::vector<float> s(n);
    for (size_t i = 0; i < n; ++i) s[i] = (float)(amp * std::sin(2.0 * std::numbers::pi * hz * (double)i / rate));
    XtHostInput in;
    in.prepare(rate);
    const std::vector<int32_t> got = through(in, s, s, (int)hz);
    ToneThrough t;
    long count = 0;
    for (long k = in.latency() + kStartup; k + kStartup < (long)got.size() / 2; ++k) {
        const double x = (double)got[(size_t)(2 * k)] * 0x1p-31;
        const double at = (double)(k - in.latency()) / kChipRate;
        t.off = std::max(t.off, std::abs(x - amp * std::sin(2.0 * std::numbers::pi * hz * at)));
        t.rms += x * x;
        ++count;
    }
    t.rms = std::sqrt(t.rms / (double)std::max(1L, count));
    return t;
}

// Within 10^-4 of full scale (−80 dB): measured 3.7e-5 at most (10 kHz at 48 kHz), where the kernel's nearest phase
// alone, its phases not interpolated, is 1.3e-4 off at 1 kHz and 1.3e-3 at 10 kHz
constexpr double kMostOff = 1e-4;

void tones(TestChecks& check) {
    for (const double rate : { 48000.0, 96000.0 })
        for (const double hz : { 1000.0, 10000.0 }) {
            const ToneThrough t = tone(rate, hz, 0.5);
            check(t.off < kMostOff, "a " + std::to_string((int)hz) + " Hz sine at " + std::to_string((int)rate) +
                                        " Hz as at 44.1 kHz, on time: off by " + std::to_string(t.off));
        }
    const ToneThrough high = tone(96000.0, 30000.0, 0.5);
    check(high.rms < 0.01 * 0.5 / std::numbers::sqrt2,
          "a 30 kHz tone at 96 kHz 40 dB down at 44.1 kHz: its RMS " + std::to_string(high.rms));
}

int extInTest(const juce::File& rom, const juce::File&) {
    TestChecks check { "extin" };
    if (!xtSynthSetRom(rom.getFullPathName().toStdString())) {
        check(false, "the image should load");
        return check.summary();
    }
    wordOf(check);
    direct(check);
    everyRate(check);
    tones(check);
    extInPluginChecks(check, rom);
    return check.summary();
}

}  // namespace

int main(int argc, char** argv) { return testMain(argc, argv, "extin", extInTest); }
