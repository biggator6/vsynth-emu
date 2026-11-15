#pragma once
// What the VC-2 ports' checks share (v210, moved out of Vc2UnvoicedCheck.cpp as it stood): rom.4 in an interpreter with
// RAM for the stack, the arguments and the buffers; the mic-like input a block at a time; and the first word of a span
// of rom.4's memory that differs from the port's.  xt_vc2in's (Vc2UnvoicedCheck.cpp) and xt_vc2legacy's
// (Vc2LegacyCheck.cpp) builds only.
#include "Sh4Interp.h"
#include "XtArgs.h"
#include "XtFloat.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <optional>
#include <random>
#include <string>
#include <vector>

inline constexpr uint32_t kVc2Sp = 0x8C0E0000, kVc2Args = 0x8C010000;   // in the RAM Vc2Fw::start maps
inline constexpr uint64_t kVc2Steps = 4000000;   // a call's: the output stage's reset clears 800,000 bytes
inline constexpr int kVc2Block = 32, kVc2Rate = 44100;
// A fading input's silence: long enough for the input stage's filters to give exactly 0 (758 to 1,095 blocks of it
// after noise at 1e-5 to full scale, measured with the flush on), which no shorter silence does
inline constexpr int kVc2FadeBlocks = 1600;

struct Vc2Fw {
    Sh4 cpu;
    // rom.4 loaded, and RAM for the stack, the arguments and the buffers, as xt_vc2params has them
    bool start(const char* rom) {
        if (!cpu.loadRom(rom)) return false;
        cpu.mapRam(0x8C000000, 0x00100000);
        return true;
    }
    bool call(uint32_t f, std::initializer_list<uint32_t> args, uint32_t* r0 = nullptr) {
        uint32_t r = 0;
        const bool ok = cpu.call(f, args, kVc2Sp, &r, kVc2Steps);
        if (r0) *r0 = r;
        return ok;
    }
};

// A check's run: its cases and seed
struct Vc2Run {
    int cases, seed;
};

// A check's start, `name <rom.4> [cases] [seed]`: the host's denormals flushed, as the SH-4 does; the arguments (40 cases,
// seed 2024); rom.4 in `fw` and the port's tables by `load`.  Nothing, said why, when it can't run.
inline std::optional<Vc2Run> vc2Start(int argc, char** argv, Vc2Fw& fw, const char* name, bool (*load)(const char*)) {
    if (argc < 2) {
        std::fprintf(stderr, "%s <rom.4> [cases] [seed]\n", name);
        return std::nullopt;
    }
    xtf::flushDenormals();
    const Vc2Run run { argc > 2 ? intArg(argv[2]).value_or(-1) : 40, argc > 3 ? intArg(argv[3]).value_or(-1) : 2024 };
    if (run.cases < 0 || run.seed < 0 || !fw.start(argv[1]) || !load(argv[1])) {
        std::fprintf(stderr, "%s: no rom.4 at %s, or an argument that isn't a number\n", name, argv[1]);
        return std::nullopt;
    }
    return run;
}

// The input, a block at a time: a kind and its level held for a stretch of blocks
struct Vc2Signal {
    std::mt19937 rng;
    bool fades = false;   // noise, then silence to the case's end
    int kind = 0, left = 0, drawn = 0;
    float level = 0.0f, hz = 0.0f;
    double phase = 0.0;
    float uniform() { return (float)rng() / 4294967296.0f; }
    void draw() {
        kind = (int)(rng() % 6);
        // a long silence (kind 5): the filters' states falling for thousands of samples, though not to 0
        left = kind == 5 ? 150 + (int)(rng() % 200) : 1 + (int)(rng() % 40);
        level = std::pow(10.0f, -5.0f * uniform());
        hz = 50.0f * std::pow(300.0f, uniform());
        if (fades) kind = drawn == 0 ? 1 : 0;
        if (fades && drawn > 0) left = kVc2FadeBlocks;
        ++drawn;
    }
    void block(float* x) {
        if (--left <= 0) draw();
        for (int i = 0; i < kVc2Block; ++i) {
            const float sine = (float)std::sin(phase);
            phase += 2.0 * std::numbers::pi * hz / kVc2Rate;
            const float noise = 2.0f * uniform() - 1.0f;
            const float kinds[6] = { 0.0f, noise, sine, 0.5f * (noise + sine), left == 1 ? 0.0f : noise, 0.0f };
            x[i] = level * kinds[kind];
        }
    }
};

// The first word of `bytes` at `at` in rom.4's memory that differs from the port's `mine`, as text; empty when none does
inline std::string vc2FirstDifference(Vc2Fw& fw, uint32_t at, const void* mine, size_t bytes, const char* what) {
    std::vector<uint32_t> theirs(bytes / 4), ours(bytes / 4);
    fw.cpu.read(at, theirs.data(), bytes);
    std::memcpy(ours.data(), mine, bytes);
    const auto [a, b] = std::ranges::mismatch(theirs, ours);
    if (a == theirs.end()) return "";
    char t[160];
    std::snprintf(t, sizeof t, "%s, word 0x%08x: %08x, against %08x", what,
                  at + 4 * (uint32_t)(a - theirs.begin()), *b, *a);
    return t;
}

// One of the port's buffers against rom.4's at `at`
inline std::string vc2BufferDifference(Vc2Fw& fw, uint32_t at, const std::vector<float>& mine, const char* what) {
    return vc2FirstDifference(fw, at, mine.data(), mine.size() * sizeof(float), what);
}
