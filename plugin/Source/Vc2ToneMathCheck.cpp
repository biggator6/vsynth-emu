// xt_vc2libm's second part (v320): Vc2ToneMath, the routines the VC-2's ports share in the plugin, against rom.4's own
// in the interpreter, every answer bit for bit.  The velocity curve (0x893a7fb0, its answer in r0) on every velocity
// 0…127, sens −260…260 and curve 0…9; exp2 (0x893a8340, fr4 to fr0) on its edges within ±7 octaves (each whole octave
// and either side of it, the fraction's 16 bits' first and last steps, either side of each) and every
// 2^kExpShift-th float pattern within them, from an offset the seed draws, both signs; logf and powf (0x895065f4,
// 0x89506e54) through the class on [cases] of xt_vc2libm's draws each (their errno is the first part's).  Not given,
// as the part gives none of them: a velocity past 127 or below 0, where rom.4 reads past its curve's 128 bytes; a curve
// below 0, where it reads a curve's pointer before its table (it tests 7 and up alone, `cmp/ge`, where rom.2's port
// answers 0x8000 below 0 too); a sens past 16 bits, which it takes by `exts.w`; exp2 from 7 octaves on, where it reads
// past its seven words into the tables after them (HARDWARE_FIRMWARE.md, "The VC-2's LITE note-on").
// XT_VC2LIBM_MAIN only.
#ifdef XT_VC2LIBM_MAIN
#include "Vc2ToneMathCheck.h"
#include "Vc2LibMathDraws.h"
#include "Vc2ToneMath.h"

namespace {
constexpr uint32_t kVelocity = 0x893a7fb0, kExp2 = 0x893a8340, kLog = 0x895065f4, kPow = 0x89506e54;   // [FW]
constexpr int kVelocities = 128, kLeastSens = -260, kMostSens = 260, kLeastCurve = 0, kMostCurve = 9;
constexpr int kExpShift = 11, kOctaves = 7;
constexpr uint32_t kSevenOctaves = 0x40e00000u;   // 7.0f: the domain's end
constexpr uint32_t kFractionSteps = 65536;       // [FW] 0x893a8340's fraction, 16 bits

struct MathRig {
    Vc2Fw& fw;
    Vc2Tally& tally;
    const Vc2ToneMath math {};
    long velocities = 0, exps = 0, logs = 0, pows = 0, calls = 0;
    bool stopped = false;
};

void outcome(MathRig& r, const std::string& what) {
    if (r.stopped) return;
    ++r.calls;
    r.stopped = vc2CaseOutcome(r.tally, what, (int)r.calls, 0);
}

void velocityOn(MathRig& r, int velocity, int sens, int curve) {
    uint32_t theirs = 0;
    ++r.velocities;
    if (!r.fw.call(kVelocity, { (uint32_t)velocity, (uint32_t)sens, (uint32_t)curve }, &theirs)) {
        return outcome(r, "the velocity curve faults: " + r.fw.cpu.error());
    }
    const int mine = r.math.velocity(velocity, sens, curve);
    char t[112] = "";
    if ((uint32_t)mine != theirs) {
        std::snprintf(t, sizeof t, "the velocity curve (%d, %d, %d): %d, against %d", velocity, sens, curve, mine,
                      (int)theirs);
    }
    outcome(r, t);
}

// rom.4's routine of one or two floats (fr4, fr5) against the class's answer
void floatOn(MathRig& r, uint32_t routine, float x, float y, float mine) {
    r.fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(x);
    r.fw.cpu.fb[0][5] = std::bit_cast<uint32_t>(y);
    if (!r.fw.call(routine, {})) return outcome(r, "rom.4's routine faults: " + r.fw.cpu.error());
    const uint32_t theirs = r.fw.cpu.fb[0][0];
    char t[128] = "";
    if (theirs != std::bit_cast<uint32_t>(mine)) {
        std::snprintf(t, sizeof t, "0x%08x (x %08x, y %08x): %08x, against %08x", routine, std::bit_cast<uint32_t>(x),
                      std::bit_cast<uint32_t>(y), std::bit_cast<uint32_t>(mine), theirs);
    }
    outcome(r, t);
}

void expOn(MathRig& r, float x) {
    ++r.exps;
    floatOn(r, kExp2, x, 0.0f, r.math.exp2(x));
}

void checkVelocity(MathRig& r) {
    for (int curve = kLeastCurve; curve <= kMostCurve && !r.stopped; ++curve) {
        for (int sens = kLeastSens; sens <= kMostSens && !r.stopped; ++sens) {
            for (int velocity = 0; velocity < kVelocities; ++velocity) velocityOn(r, velocity, sens, curve);
        }
    }
}

// Each whole octave within ±7, the fraction's first and last steps after it, and a float either side of each
void checkExpEdges(MathRig& r) {
    const float step = 1.0f / (float)kFractionSteps;
    for (int n = 1 - kOctaves; n < kOctaves; ++n) {
        for (float at : { (float)n, (float)n + step, (float)n + 1.0f - step }) {
            for (float x : { at, std::nextafter(at, -1e9f), std::nextafter(at, 1e9f) }) {
                if (std::fabs(x) < (float)kOctaves) expOn(r, x);
            }
        }
    }
    expOn(r, -0.0f);
}

void checkExpSweep(MathRig& r, std::mt19937& rng) {
    const uint32_t offset = rng() & ((1u << kExpShift) - 1);
    for (uint32_t p = offset; p < kSevenOctaves && !r.stopped; p += 1u << kExpShift) {
        expOn(r, std::bit_cast<float>(p));
        expOn(r, std::bit_cast<float>(p | kLibSign));
    }
}

void checkLogPow(MathRig& r, const Vc2Run& run, std::mt19937& rng) {
    for (int c = 0; c < run.cases && !r.stopped; ++c) {
        const float x = libDrawLog(rng);
        ++r.logs;
        floatOn(r, kLog, x, 0.0f, r.math.log(x));
        const LibPower p = libDrawPow(rng);
        ++r.pows;
        floatOn(r, kPow, p.x, p.y, r.math.pow(p.x, p.y));
    }
}
} // namespace

void vc2ToneMathPart(Vc2Fw& fw, const Vc2Run& run, Vc2Tally& tally) {
    MathRig r { fw, tally };
    std::mt19937 rng((uint32_t)run.seed + 1);
    checkVelocity(r);
    checkExpEdges(r);
    checkExpSweep(r, rng);
    checkLogPow(r, run, rng);
    std::printf("shared: %ld velocity curves, %ld exp2s within ±7 octaves, %ld logfs, %ld powfs through Vc2ToneMath\n",
                r.velocities, r.exps, r.logs, r.pows);
}
#endif
