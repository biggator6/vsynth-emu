// xt_vc2tracker <rom.4> [cases] [seed] (v223, v224): the VC-2's pitch tracker, Vc2Tracker with Vc2TrackerAnalysis,
// against rom.4's own module on its state in rom.4's .bss (0x899d9b70…0x899d9d1c): its two static initializers
// (0x893edf90, 0x893ef240) once, then each case the reset (0x893ee040) on both, now and then every word of the state
// drawn on both (so the locked paths are reached at once), and blocks of the tracker's input (Vc2TrackerSignal: held
// tones of 55 to 1760 Hz with up to five harmonics, from 1e-4 to full scale, noise, silence and Vc2Signal's short
// stretches) through the block entry (0x893ee280), a block mostly 32 samples and now and then fewer; between blocks
// now and then the mode setter (0x893ee100, −1…4) or the noisy-hold setter (0x893ee030).  The whole module runs on
// both, the crossing routines, the onset check, the estimator and taking (v224) as well as the frame.  After each
// routine the state word for word, after a block the period returned and the noisy-hold getter (0x893ee020).  It counts
// the crossings, the blocks P moved in, the blocks locked and the blocks of a tone with P within 3% of its period.
// XT_VC2TRACKER_FIRST=1 says where the first case to differ does, XT_VC2TRACKER_STOP=1 stops there.
// XT_VC2TRACKER_MAIN only.
#ifdef XT_VC2TRACKER_MAIN
#include "Vc2CheckRig.h"
#include "Vc2TrackerAnalysis.h"
#include "Vc2TrackerSignal.h"
#include <cmath>
#include <cstdio>

namespace {
constexpr uint32_t kStatic = 0x893edf90, kSetter = 0x893ef240, kReset = 0x893ee040, kMode = 0x893ee100;   // [FW]
constexpr uint32_t kBlockEntry = 0x893ee280, kGetNoisy = 0x893ee020, kSetNoisy = 0x893ee030;              // [FW]
constexpr uint32_t kStateAt = 0x899d9b70;                                                                  // [FW]
constexpr uint32_t kInputAt = 0x8C035000;   // in Vc2Fw's RAM
constexpr int kBlocks = 1500, kEventOneIn = 60, kScrambleOneIn = 200, kJoltOneIn = 100;
constexpr int32_t kLocked = 3;              // [FW] a lock above it is locked
constexpr float kOnPitch = 0.03f;           // P within this of a tone's period counts as on pitch
constexpr uint32_t kCounts = 7;             // the crossings' counts drawn, 0…6

// The port's analysis, its crossings counted
class Counting final : public Vc2TrackerCrossings {
public:
    void up(Vc2TrackerWords& s, float previous, float now) override {
        ++ups;
        analysis_.up(s, previous, now);
    }
    void down(Vc2TrackerWords& s, float previous, float now) override {
        ++downs;
        analysis_.down(s, previous, now);
    }
    long ups = 0, downs = 0;

private:
    Vc2TrackerAnalysis analysis_;
};

struct Counts {
    long blocks = 0, moves = 0, locked = 0, toneBlocks = 0, onPitch = 0, modes = 0, holds = 0, noisy = 0;
};

// Both sides: rom.4 in its interpreter, the port with its counted analysis, and what has been counted
struct Sides {
    Vc2Fw& fw;
    Vc2Tracker& tracker;
    Counting& analysis;
    Counts n;
};

std::string trackerDifference(Vc2Fw& fw, const Vc2Tracker& t) {
    return vc2FirstDifference(fw, kStateAt, &t.s, sizeof t.s, "the tracker's words");
}

// A side's words as a sounding input leaves them: peaks up to a tenth of full scale
Vc2TrackerSide drawSide(std::mt19937& rng) {
    return { 0.1f * vc2Unit(rng), 0.1f * vc2Unit(rng), 0.1f * vc2Unit(rng), 0.1f * vc2Unit(rng) };
}

// A period from 20 to 1000 samples, by octaves
float drawPeriod(std::mt19937& rng) { return 20.0f * std::pow(50.0f, vc2Unit(rng)); }

// A period near the tone's (none: any), or near a multiple or fraction of it, as an octave error leaves P
float nearPeriod(std::mt19937& rng, float tonePeriod) {
    constexpr float kMultiples[] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.0f, 1.5f, 2.0f, 4.0f, 5.0f, 6.0f };
    if (!(tonePeriod > 0.0f) || rng() % 4 == 0) return drawPeriod(rng);
    return tonePeriod * kMultiples[rng() % std::size(kMultiples)] * (0.97f + 0.06f * vc2Unit(rng));
}

// The half-intervals as an uneven wave of the period leaves them, two values alternating, the oldest now and then off
void drawHalves(Vc2TrackerWords& s, std::mt19937& rng, float period) {
    const float a = period * (0.2f + 0.6f * vc2Unit(rng)), b = period - a;
    s.halves[0] = b;
    s.halves[1] = a;
    s.halves[2] = rng() % 3 == 0 ? b * (0.5f + vc2Unit(rng)) : b;
    s.halves[3] = a;
}

// A lock: none, a few agreeing estimates, near the cap of 256 (so agreeing ones meet it), or any up to it
int32_t drawLock(std::mt19937& rng) {
    constexpr int32_t kCap = 256;
    constexpr int32_t kLocks[][2] = { { 0, 1 }, { 1, 5 }, { kCap - 4, 5 }, { 0, kCap + 1 } };   // from, how many
    const auto& [from, count] = kLocks[rng() % std::size(kLocks)];
    return from + (int32_t)(rng() % (uint32_t)count);
}

// The analysis's words: the sides, the half-intervals, the counts, the candidates near them and the lock
void scrambleAnalysis(Vc2TrackerWords& s, std::mt19937& rng, float tonePeriod) {
    s.down = drawSide(rng);
    s.up = drawSide(rng);
    drawHalves(s, rng, nearPeriod(rng, tonePeriod));
    const float half = 0.5f * (s.halves[0] + s.halves[1]);
    s.crossings = (int32_t)(rng() % kCounts);   // either side of the estimator's 3
    s.crossingsAtDown = (int32_t)(rng() % kCounts);
    s.crossingsAtUp = (int32_t)(rng() % kCounts);
    s.estimate = 2.0f * half * (0.95f + 0.1f * vc2Unit(rng));
    s.candidate = 2.0f * half * (0.95f + 0.1f * vc2Unit(rng));
    s.fraction = vc2Unit(rng);
    s.unread = vc2Unit(rng);
    s.smoothing = rng() % 2 == 0 ? 0.5f : 1.0f;
    s.lastUp = (int32_t)(rng() % 2);
    s.handOff = (int32_t)(rng() % 2);
    s.follow = (int32_t)(rng() % 3);
    s.failures = (int32_t)(rng() % 2 == 0 ? rng() % 3 : rng() % 300);   // under 3: a small lock outlives a refusal
    s.lock = drawLock(rng);
    s.peak = 0.1f * vc2Unit(rng);
    s.framePeak = s.lastFramePeak = 0.1f * vc2Unit(rng);
}

// Every word of the state but the settings and the filters, drawn on the port and copied to rom.4's, around the
// tone sounding
void scrambleTracker(Sides& b, std::mt19937& rng, float tonePeriod) {
    Vc2TrackerWords& s = b.tracker.s;
    scrambleAnalysis(s, rng, tonePeriod);
    s.lastSince = (int32_t)(rng() % 300);
    s.sinceCrossing = (int32_t)(rng() % 400);   // as a valid crossing leaves it: P stays apart from the fallback a while
    s.sinceUp = (int32_t)(rng() % 800);
    s.sinceDown = (int32_t)(rng() % 800);
    s.period = rng() % 4 == 0 ? s.fallback : nearPeriod(rng, tonePeriod);
    s.fallback = rng() % 2 == 0 ? s.period : drawPeriod(rng);
    s.recentPeak = 0.05f * vc2Unit(rng);
    s.noise = (int32_t)(rng() % 40);
    s.noisyHold = (int32_t)(rng() % 2);
    s.noisyHoldOn = rng() % 4 == 0 ? 0 : 1;
    s.lowpassTarget = 0.12f * vc2Unit(rng);
    s.highpassTarget = 0.06f * vc2Unit(rng);
    b.fw.cpu.write(kStateAt, &s, sizeof s);
}

// The stored halves around the tone's: the one the coming half is held to true, the other two off by a multiple, so
// the halves disagree with the candidate far from P, or near it
void joltHalves(Vc2TrackerWords& s, std::mt19937& rng, float tonePeriod) {
    constexpr float kOff[] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f };
    const float half = 0.5f * tonePeriod;
    s.halves[0] = half * kOff[rng() % std::size(kOff)];
    s.halves[1] = half * (0.9f + 0.2f * vc2Unit(rng));
    s.halves[2] = half * (0.25f + 3.0f * vc2Unit(rng));
}

// P moved off the tone it tracks, with a new lock and few refusals, now and then the halves, the rest as the signal
// left it: so an estimate agreeing on the tone meets a lock of 1 to 3 or a long one, and P far from it
void joltTracker(Sides& b, std::mt19937& rng, float tonePeriod) {
    Vc2TrackerWords& s = b.tracker.s;
    s.period = s.estimate = nearPeriod(rng, tonePeriod);
    s.lock = drawLock(rng);
    s.failures = (int32_t)(rng() % 3);
    if (tonePeriod > 0.0f && rng() % 2 == 0) joltHalves(s, rng, tonePeriod);
    b.fw.cpu.write(kStateAt, &s, sizeof s);
}

// What a block leaves to count: P moved, locked, and on a tone's period
void countBlock(Sides& b, float before, float tonePeriod) {
    const Vc2TrackerWords& s = b.tracker.s;
    ++b.n.blocks;
    b.n.moves += s.period != before ? 1 : 0;
    b.n.locked += s.lock > kLocked ? 1 : 0;
    b.n.noisy += s.noisyHold != 0 ? 1 : 0;
    if (!(tonePeriod > 0.0f)) return;
    ++b.n.toneBlocks;
    b.n.onPitch += std::fabs(s.period - tonePeriod) < kOnPitch * tonePeriod ? 1 : 0;
}

// One block of n samples on both, then the state, the return and the getter
std::string trackerBlockOn(Sides& b, const float* x, int n, float tonePeriod) {
    b.fw.cpu.write(kInputAt, x, sizeof(float) * (size_t)n);
    const float before = b.tracker.s.period;
    float returned = 0.0f;
    std::string what = vc2BothOn(b.fw, kBlockEntry, { kInputAt, (uint32_t)n }, "the block entry",
                                 [&] { returned = b.tracker.block(x, n); },
                                 [&] { return trackerDifference(b.fw, b.tracker); });
    countBlock(b, before, tonePeriod);
    if (what.empty() && !vc2SameAsFr0(b.fw, returned)) what = "the period returned";
    uint32_t noisy = 0;
    if (what.empty() && !b.fw.call(kGetNoisy, {}, &noisy)) what = "the getter faults: " + b.fw.cpu.error();
    if (what.empty() && (int32_t)noisy != b.tracker.noisyHold()) what = "the noisy-hold getter";
    return what;
}

// An event between blocks: the mode setter (−1…4) or the noisy-hold setter, on both
std::string trackerEventOn(Sides& b, std::mt19937& rng) {
    Vc2Tracker& t = b.tracker;
    const auto differs = [&] { return trackerDifference(b.fw, t); };
    if (rng() % 2 == 0) {
        ++b.n.modes;
        const int mode = (int)(rng() % 6) - 1;
        return vc2BothOn(b.fw, kMode, { (uint32_t)mode }, "the mode setter", [&] { t.setMode(mode); }, differs);
    }
    ++b.n.holds;
    const int32_t on = (int32_t)(rng() % 2);
    return vc2BothOn(b.fw, kSetNoisy, { (uint32_t)on }, "the noisy-hold setter", [&] { t.setNoisyHold(on); },
                     differs);
}

// The case's blocks after its reset, each with what may come before it
std::string trackerCase(Sides& b, std::mt19937& rng, bool fades) {
    Vc2TrackerSignal input(rng(), fades);
    std::string what = vc2ResetOn(b.fw, kReset, {}, b.tracker, trackerDifference);
    if (what.empty() && rng() % 2 == 0) scrambleTracker(b, rng, 0.0f);
    float x[kVc2Block];
    for (int k = 0; k < kBlocks && what.empty(); ++k) {
        if (rng() % kScrambleOneIn == 0) scrambleTracker(b, rng, input.tonePeriod());
        if (rng() % kJoltOneIn == 0) joltTracker(b, rng, input.tonePeriod());
        if (rng() % kEventOneIn == 0) what = trackerEventOn(b, rng);
        input.block(x);
        const int samples = rng() % 8 == 0 ? 1 + (int)(rng() % kVc2Block) : kVc2Block;
        if (what.empty()) what = trackerBlockOn(b, x, samples, input.tonePeriod());
    }
    return what;
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2tracker", vc2LoadTracker);
    if (!run) return 1;
    Counting analysis;
    Vc2Tracker tracker(analysis);
    Sides b { fw, tracker, analysis, {} };
    std::string what;
    if (!fw.call(kStatic, {}) || !fw.call(kSetter, {})) what = "the static initializers fault: " + fw.cpu.error();
    if (what.empty()) what = trackerDifference(fw, tracker);
    if (!what.empty()) {
        std::printf("  after the static initializers: %s\nxt_vc2tracker: 1 different\n", what.c_str());
        return 0;
    }
    std::mt19937 rng((uint32_t)run->seed);
    Vc2Tally tally { "XT_VC2TRACKER_FIRST", "XT_VC2TRACKER_STOP" };
    for (int c = 0; c < run->cases; ++c) {
        if (vc2CaseOutcome(tally, trackerCase(b, rng, c % 8 == 7), c, b.n.blocks)) break;
    }
    const Counts& n = b.n;
    std::printf("tracker: %d cases, %ld blocks, %ld up and %ld down crossings, P moved in %ld, %ld locked, "
                "%ld of %ld tone blocks on pitch, %ld modes, %ld holds, %ld noisy, %ld different\n", run->cases,
                n.blocks, analysis.ups, analysis.downs, n.moves, n.locked, n.onPitch, n.toneBlocks, n.modes, n.holds,
                n.noisy, tally.different);
    std::printf("xt_vc2tracker: %ld different\n", tally.different);
    return 0;
}
#endif
