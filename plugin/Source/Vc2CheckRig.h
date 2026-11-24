#pragma once
// What the VC-2 ports' checks share (v210, moved out of Vc2UnvoicedCheck.cpp as it stood): rom.4 in an interpreter with
// RAM for the stack, the arguments and the buffers; the mic-like input a block at a time; and the first word of a span
// of rom.4's memory that differs from the port's.  xt_vc2in's (Vc2UnvoicedCheck.cpp) and xt_vc2legacy's
// (Vc2LegacyCheck.cpp) builds only.
#include "Sh4Interp.h"
#include "XtArgs.h"
#include "XtFloat.h"
#include "XtSwitches.h"
#include <bit>
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
    // rom.4 loaded, and RAM for the stack, the arguments and the buffers, as xt_vc2params has them; from v335 2 MB,
    // the period-jump player's waves in xt_vc2osc above the first (its read leads the walk past a wave)
    bool start(const char* rom) {
        if (!cpu.loadRom(rom)) return false;
        cpu.mapRam(0x8C000000, 0x00200000);
        return true;
    }
    bool call(uint32_t f, std::initializer_list<uint32_t> args, uint32_t* r0 = nullptr) {
        uint32_t r = 0;
        const bool ok = cpu.call(f, args, kVc2Sp, &r, kVc2Steps);
        if (r0) *r0 = r;
        return ok;
    }
};

// The Hold pedal's 0…127: its two ends most of the time, as a pedal gives; 64 and 63, where the hold's branches part
// (v210's, here from v216, the draws as they were)
inline int vc2DrawController(std::mt19937& rng) {
    constexpr int kPicks[10] = { 127, 127, 127, 127, 0, 0, 0, 64, 63, -1 };   // -1: any
    const int pick = kPicks[rng() % 10];
    return pick < 0 ? (int)(rng() % 128) : pick;
}

// A dump time: its ends most often (127 and 126 put the pedal's hold either side of the freeze at 0.99, 0 holds nothing)
inline float vc2DrawDump(std::mt19937& rng) {
    constexpr int kDumps[6] = { 127, 126, 0, -1, -1, -1 };   // -1: any
    const int pick = kDumps[rng() % 6];
    return (float)(pick < 0 ? (int)(rng() % 128) : pick) / 127.0f;
}

// A draw in [0, 1) (v221's, here from v223)
inline float vc2Unit(std::mt19937& rng) { return (float)rng() / 4294967296.0f; }

// A level as a parameter's 0…127 gives it, v/127 (v220's, here from v221)
inline float vc2DrawLevel(std::mt19937& rng) { return (float)(rng() % 128) / 127.0f; }

// A period from 20 to 1000 samples, by octaves (v223's, here from v266)
inline float vc2DrawPeriod(std::mt19937& rng) { return 20.0f * std::pow(50.0f, vc2Unit(rng)); }

// A Formant Shift ratio, 0.5 to 2: its ends and 1 most often (v211's)
inline float vc2DrawRatio(std::mt19937& rng) {
    constexpr float kRatios[6] = { 0.5f, 1.0f, 2.0f, -1.0f, -1.0f, -1.0f };   // -1: any
    const float pick = kRatios[rng() % 6];
    return pick < 0.0f ? 0.5f + 1.5f * (float)rng() / 4294967296.0f : pick;
}

// A getter's float result, as rom.4 leaves it in fr0, against the port's (v211's, here from v215)
inline bool vc2SameAsFr0(const Vc2Fw& fw, float mine) { return fw.cpu.fb[0][0] == std::bit_cast<uint32_t>(mine); }

// A check's run: its cases and seed
struct Vc2Run {
    int cases, seed;
};

// A check's loader: each port's tables it reads, stopping at the first that can't load (v266)
template <auto... Load>
bool vc2LoadAll(const char* path) {
    return (Load(path) && ...);
}

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

inline constexpr float kVc2Bus = 1073741824.0f;   // a carrier's scale, the bus's (2^30)

// The carriers at the bus's scale (v211's, here from v217): the left's in each frame's first word; the right's, when
// there is one, in the second, else something mono leaves
inline void vc2CarrierFrames(Vc2Signal& left, Vc2Signal* right, float* frames) {
    float x[kVc2Block], r[kVc2Block];
    left.block(x);
    if (right) right->block(r);
    for (int i = 0; i < kVc2Block; ++i) {
        frames[2 * i] = x[i] * kVc2Bus;
        frames[2 * i + 1] = right ? r[i] * kVc2Bus : -0.5f * x[i];
    }
}

// The first word of `bytes` at `at` in rom.4's memory that differs from the port's `mine`, as text; empty when none does.
// v348: the words rounded up, so a region that isn't whole words (the sets' counts, 6 bytes) is read and compared to
// its last byte on both sides, the rest of its last word 0 on both, not written past
inline std::string vc2FirstDifference(Vc2Fw& fw, uint32_t at, const void* mine, size_t bytes, const char* what) {
    std::vector<uint32_t> theirs((bytes + 3) / 4), ours((bytes + 3) / 4);
    fw.cpu.read(at, theirs.data(), bytes);
    std::memcpy(ours.data(), mine, bytes);
    const auto [a, b] = std::ranges::mismatch(theirs, ours);
    if (a == theirs.end()) return "";
    char t[160];
    std::snprintf(t, sizeof t, "%s, word 0x%08x: %08x, against %08x", what,
                  at + 4 * (uint32_t)(a - theirs.begin()), *b, *a);
    return t;
}

// A region of rom.4's memory and the port's words for it, and its name (v226)
struct Vc2Region {
    uint32_t at;
    const void* mine;
    size_t bytes;
    const char* what;
};

// The first of the regions, in order, whose words differ, as vc2FirstDifference says it; empty when none does (v226: kind
// 6's and Auto Note's checks)
inline std::string vc2RegionsDifference(Vc2Fw& fw, std::initializer_list<Vc2Region> regions) {
    for (const Vc2Region& r : regions) {
        std::string what = vc2FirstDifference(fw, r.at, r.mine, r.bytes, r.what);
        if (!what.empty()) return what;
    }
    return "";
}

// A reset on both, rom.4's routine (v211) and the port's reset(), then `difference(fw, port)`; empty when nothing differs
template <class Port, class Difference>
std::string vc2ResetOn(Vc2Fw& fw, uint32_t routine, std::initializer_list<uint32_t> args, Port& port,
                       Difference difference) {
    if (!fw.call(routine, args)) return "the reset faults: " + fw.cpu.error();
    port.reset();
    return difference(fw, port);
}

// An analysis's cases (v215, LEGACY's of v210 and HIRESO's; HIRESO's object's, v216): rom.4's init, each case's start;
// every `fadeEvery`th case fading into silence, the others `blocks` blocks; the cases' draws; the init's argument, an
// object's address (0 for an analysis's, whose init takes none); and the blocks run
struct Vc2Cases {
    uint32_t init;
    int fadeEvery, blocks;
    std::mt19937 rng;
    uint32_t self = 0;
    long blocksRun = 0;
};

// Case `c` of an analysis's: both reset by rom.4's init and the port's reset(), then the case's blocks of Vc2Signal's
// input, each through `block(x)`, which says what differs; empty when nothing does
template <class Port, class Difference, class Block>
std::string vc2AnalysisCase(Vc2Fw& fw, Port& port, Difference difference, Vc2Cases& cases, int c, Block block) {
    const bool fades = c % cases.fadeEvery == cases.fadeEvery - 1;
    Vc2Signal input { std::mt19937(cases.rng()), fades };
    std::string what = vc2ResetOn(fw, cases.init, { cases.self }, port, difference);
    float x[kVc2Block];
    for (int k = 0; k < (fades ? kVc2FadeBlocks : cases.blocks) && what.empty(); ++k, ++cases.blocksRun) {
        input.block(x);
        what = block(x);
    }
    return what;
}

// An event on both (v220): rom.4's `routine` on `args` and the port's `mine()`, then `differs()`; that the routine
// `name`d faulted, or what differs, or nothing
template <class Mine, class Differs>
std::string vc2BothOn(Vc2Fw& fw, uint32_t routine, std::initializer_list<uint32_t> args, const char* name, Mine mine,
                      Differs differs) {
    if (!fw.call(routine, args)) return std::string(name) + " faults: " + fw.cpu.error();
    mine();
    return differs();
}

// A check's tally of the cases that differ (v223, the checks' case loops'): its FIRST switch, which says where the first
// case to differ does, and its STOP switch, which stops there (a plant's run: its first case that differs is enough)
struct Vc2Tally {
    const char* firstSwitch;
    const char* stopSwitch;
    long different = 0;
};

// Case c's outcome after `blocks` blocks: nothing when `what` is empty; else counted, the first said under the FIRST
// switch; true when the STOP switch ends the run there
inline bool vc2CaseOutcome(Vc2Tally& tally, const std::string& what, int c, long blocks) {
    if (what.empty()) return false;
    if (!tally.different++ && xtSwitch(tally.firstSwitch))
        std::printf("  case %d, block %ld: %s\n", c, blocks, what.c_str());
    return xtSwitch(tally.stopSwitch) != nullptr;
}

// A check's cases after its start (v225: the tracker's and kind 6's): a difference at the start, said and counted as
// one case; else each case on the run's seed, every eighth fading, `oneCase(rng, fades)` saying what differs, through
// vc2CaseOutcome after `blocks` blocks
template <class Case>
void vc2RunCases(const Vc2Run& run, Vc2Tally& tally, const std::string& atStart, const long& blocks, Case oneCase) {
    if (!atStart.empty()) {
        std::printf("  at the start: %s\n", atStart.c_str());
        tally.different = 1;
        return;
    }
    std::mt19937 rng((uint32_t)run.seed);
    for (int c = 0; c < run.cases; ++c) {
        if (vc2CaseOutcome(tally, oneCase(rng, c % 8 == 7), c, blocks)) break;
    }
}

// One of the port's buffers against rom.4's at `at`
inline std::string vc2BufferDifference(Vc2Fw& fw, uint32_t at, const std::vector<float>& mine, const char* what) {
    return vc2FirstDifference(fw, at, mine.data(), mine.size() * sizeof(float), what);
}
