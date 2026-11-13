// vc2run <rom.4> track: the VC-2's pitch tracker run on rom.4's own routines (research/VC2_VOCAL_DESIGNER.md, "The pitch
// tracker, run"), as the kind 6 run runs its pitch shifter: the tracker's two global setters from the init table
// (0x893edf90, 0x893ef240), its reset (0x893ee040) and a mode (0x893ee100: four voice ranges), then 32-sample blocks of a
// known signal into its block entry (0x893ee280(samples, n), which returns a float), the period it keeps at 0x899d9bd0
// (what 0x893ee010 returns, in samples) read after each.  The input is in the firmware's units: the glue (0x893b1880)
// gives the tracker the frame's int32 word × 2^-31, so 1 is full scale.  Says, for tones of four harmonics, noise and
// silence: the pitch it ends on, its return over the last blocks, and the first block from which it stays within 2% of
// the tone's.  Four sections: every mode on a coarse grid; then, in mode 0, the only one the firmware selects (the reset
// passes 0, and the mode setter has no other caller and no pointer to it), every note over five octaves, levels in 6 dB
// steps at four pitches, and a second of one input followed by a second of another.
#include "vc2runs.h"
#include "vc2twin.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kSetGlobals = 0x893edf90, kSetTables = 0x893ef240;   // [FW] the init table's (0x898c60a8)
constexpr uint32_t kReset = 0x893ee040, kSetMode = 0x893ee100, kTrackBlock = 0x893ee280;   // [FW]
constexpr uint32_t kPeriod = 0x899d9bd0;   // [FW] the tracked period, samples
constexpr uint32_t kFirmwareMode = 0;      // [FW] the reset's r4 (0x893ee04a), the mode setter's only call
// [FW] what the mode setter writes as 2πf/44100, two words each
constexpr uint32_t kModeOmegas[] = {0x899d9bc8, 0x899d9cb8, 0x899d9bcc, 0x899d9cb4};
constexpr uint32_t kIn = kScratch, kModes = 4;
constexpr int kBlock = 32, kSecond = 1400, kTail = 100;   // a block's samples; blocks: about a second, the return's mean
constexpr double kRate = 44100, kNear = 0.02, kOvertone = 0.6;
constexpr int kHarmonics = 4;
constexpr double kPitches[] = {55, 82.5, 110, 165, 220, 330, 440, 660, 880};
constexpr double kLevels[] = {0.001, 0.05, 0.5};
constexpr int kLowNote = 28, kHighNote = 92;   // the sweep's MIDI notes, 41 Hz to 1661 Hz
constexpr double kLoud = 0.5, kQuiet = 0.001;
constexpr double kLevelPitches[] = {110, 220, 440, 880};
constexpr int kLowDb = -66, kDbStep = 6;

enum class Kind { tone, noise, silence };
constexpr const char* kKindNames[] = {"tone", "noise", "silence"};
struct Input {
    Kind kind;
    double hz, level;
};
struct Segment {
    Input in;
    int blocks;
};
struct Readings {
    std::vector<double> hz, ret;   // after each block: 44100 / the period, and the block entry's return
    bool ok = false;
};
struct Change {
    Input before, after;
};
constexpr Change kChanges[] = {
    {{Kind::tone, 220, kLoud}, {Kind::silence, 0, 0}}, {{Kind::tone, 220, kLoud}, {Kind::noise, 0, kLoud}},
    {{Kind::tone, 220, kLoud}, {Kind::tone, 220, kQuiet}}, {{Kind::tone, 110, kLoud}, {Kind::tone, 330, kLoud}},
    {{Kind::tone, 330, kLoud}, {Kind::tone, 110, kLoud}}, {{Kind::tone, 220, kLoud}, {Kind::tone, 440, kLoud}},
    {{Kind::tone, 440, kLoud}, {Kind::tone, 220, kLoud}}, {{Kind::tone, 110, kLoud}, {Kind::tone, 880, kLoud}},
};

// One sample of the input: four harmonics falling by kOvertone each, white noise, or nothing
double sampleOf(const Input& in, long n, Rng& r) {
    if (in.kind == Kind::silence) return 0;
    if (in.kind == Kind::noise) return in.level * r.in(-1.0f, 1.0f);
    double v = 0, a = 1;
    for (int k = 1; k <= kHarmonics; ++k, a *= kOvertone)
        v += a * std::sin(2 * std::numbers::pi * in.hz * k * (double)n / kRate + k);
    return in.level * v / 2;
}

// What a routine returned as a float (call() leaves FPSCR.FR at 0: bank 0 holds FR0)
double fr0Of(Machine& m) {
    float f = 0;
    std::memcpy(&f, &m.cpu.fb[0][0], 4);
    return f;
}

// The tracker reset into `mode`, then each segment's blocks in turn
Readings readingsOf(Machine& m, uint32_t mode, const std::vector<Segment>& signal) {
    Readings out;
    Rng r;
    if (!m.run(kReset, {}) || !m.run(kSetMode, {mode})) return out;
    long n = 0;
    for (const Segment& s : signal)
        for (int b = 0; b < s.blocks; ++b) {
            for (int k = 0; k < kBlock; ++k, ++n) m.setF32(kIn + 4 * (uint32_t)k, (float)sampleOf(s.in, n, r));
            if (!m.run(kTrackBlock, {kIn, (uint32_t)kBlock})) return out;
            out.hz.push_back(kRate / m.f32(kPeriod));
            out.ret.push_back(fr0Of(m));
        }
    out.ok = true;
    return out;
}

// Blocks after `first` from which every reading stays within kNear of `target`, or −1
int settledFrom(const std::vector<double>& hz, size_t first, double target) {
    int from = -1;
    for (size_t b = hz.size(); b > first && std::fabs(hz[b - 1] / target - 1) < kNear; --b) from = (int)(b - 1 - first);
    return from;
}

double msOf(int blocks) {
    return blocks * kBlock * 1000 / kRate;
}

double tailMean(const std::vector<double>& v) {
    double sum = 0;
    for (size_t k = v.size() - kTail; k < v.size(); ++k) sum += v[k];
    return sum / kTail;
}

// The end of a line: when it settled, if it did
void saySettled(int from) {
    if (from >= 0) std::printf(", within 2%% from block %d (%.0f ms)", from, msOf(from));
    std::printf("\n");
}

// Each input for a second from a reset into `mode`, a line each; false on a fault
bool runEach(Machine& m, uint32_t mode, const std::vector<Input>& inputs) {
    for (const Input& in : inputs) {
        const Readings r = readingsOf(m, mode, {{in, kSecond}});
        if (!r.ok) return false;
        std::printf("  %-7s %7.1f Hz at %8.6f: ends at %8.2f Hz, returns %10.4g", kKindNames[(int)in.kind], in.hz,
                    in.level, r.hz.back(), tailMean(r.ret));
        saySettled(in.kind == Kind::tone ? settledFrom(r.hz, 0, in.hz) : -1);
    }
    return true;
}

bool everyMode(Machine& m) {
    for (uint32_t mode = 0; mode < kModes; ++mode) {
        if (!m.run(kReset, {}) || !m.run(kSetMode, {mode})) return false;
        std::printf("mode %u: starts at %.2f Hz; sets f =", mode, kRate / m.f32(kPeriod));
        for (const uint32_t at : kModeOmegas) std::printf(" %.2f", m.f32(at) * kRate / (2 * std::numbers::pi));
        std::printf(" Hz\n");
        std::vector<Input> inputs;
        for (const double hz : kPitches)
            for (const double level : kLevels) inputs.push_back({Kind::tone, hz, level});
        inputs.push_back({Kind::noise, 0, kLoud});
        inputs.push_back({Kind::silence, 0, 0});
        if (!runEach(m, mode, inputs)) return false;
    }
    return true;
}

bool everyNote(Machine& m) {
    std::printf("\nmode %u, notes %d…%d at %.1f\n", kFirmwareMode, kLowNote, kHighNote, kLoud);
    std::vector<Input> inputs;
    for (int note = kLowNote; note <= kHighNote; ++note)
        inputs.push_back({Kind::tone, 440 * std::exp2((note - 69) / 12.0), kLoud});
    return runEach(m, kFirmwareMode, inputs);
}

bool everyLevel(Machine& m) {
    std::printf("\nmode %u, levels %d…0 dB of full scale by %d dB\n", kFirmwareMode, kLowDb, kDbStep);
    std::vector<Input> inputs;
    for (const double hz : kLevelPitches)
        for (int db = kLowDb; db <= 0; db += kDbStep) inputs.push_back({Kind::tone, hz, std::pow(10.0, db / 20.0)});
    return runEach(m, kFirmwareMode, inputs);
}

bool everyChange(Machine& m) {
    std::printf("\nmode %u, a second of one input then a second of another (blocks counted from the change)\n",
                kFirmwareMode);
    for (const Change& c : kChanges) {
        const Readings r = readingsOf(m, kFirmwareMode, {{c.before, kSecond}, {c.after, kSecond}});
        if (!r.ok) return false;
        std::printf("  %s %.1f Hz at %.3f, then %s %.1f Hz at %.3f: %.2f Hz at the change, ends at %.2f Hz",
                    kKindNames[(int)c.before.kind], c.before.hz, c.before.level, kKindNames[(int)c.after.kind],
                    c.after.hz, c.after.level, r.hz[kSecond - 1], r.hz.back());
        saySettled(c.after.kind == Kind::tone ? settledFrom(r.hz, kSecond, c.after.hz) : -1);
    }
    return true;
}

} // namespace

int track(Machine& m, int, char**) {
    if (!m.run(kSetGlobals, {}) || !m.run(kSetTables, {})) {
        std::printf("track: the setters faulted: %s\n", m.error().c_str());
        return 1;
    }
    std::printf("after the setters: %.2f Hz\n", kRate / m.f32(kPeriod));
    if (everyMode(m) && everyNote(m) && everyLevel(m) && everyChange(m)) return 0;
    std::printf("track: faulted: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
