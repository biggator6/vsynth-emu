// vc2run tva <rom.2>: the patch TVA's render and end test on rom.2's and rom.4's routines, without the port (xt_tone holds
// XtSynth's TVA to rom.2, so rom.2 equal to rom.4 is the port equal to rom.4).  rom.4 compiled the TVA anew around a render
// of its own (research/VC2_VOCAL_DESIGNER.md, "The TVA in rom.4"): `0x893b2640` ramps the two gains as rom.2's render
// (0x894f5720) does and adds the signal × the gains into one stereo pair, 8 bytes a frame, where rom.2's adds it into the
// eight buses through their gliding levels.  rom.2's copy of that render is in rom.4 too (0x893b2870), but nothing calls it.
// So rom.2's render runs with the MAIN pair's levels at 1 (block +0x20, +0x24 and gliding +0x70, +0x74), where it is the
// gains alone, beside rom.4's live one; rom.4's object holds the same words 0x10 further on (its end test reads +0x50,
// +0x78, +0x7c, +0xa4 where rom.2's reads +0x40, +0x68, +0x6c, +0x94).  A state as the note-on and update leave it, then 40
// blocks of 32 frames at one of xt_cosm's levels into the pair the other voices have filled, then each image's end test:
// the pair, the targets and gains, and the end test's answer compared after each (not the steps: rom.4's render writes a
// step it shortens near the target back, rom.2's keeps it in a register, and the next update sets both).  Every tenth block
// the targets and steps move, as an update moves them.  Then twice more: with rom.2's ramp constant made rom.4's (1/32.5,
// which rom.2 holds a unit low, 0x3cfc0fc0, and rom.4 rounded to nearest, 0x3cfc0fc1), and with that and rom.4's steps set
// to rom.2's before each block.  Fails on any difference as the images are.
#include "vc2runs.h"
#include "vc2twin.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>

namespace vc2 {

namespace {

constexpr uint32_t kRender2 = 0x894f5720, kRender4 = 0x893b2640;   // [FW] rom.2's render, rom.4's live one
constexpr Twin::Pair kDone{0x894f54b0, 0x893b2560};                  // [FW] the end test: 1.000 alike by field shape
constexpr uint32_t kShift = 0x10;   // [FW] rom.4's object: rom.2's words this much further on (the end test's offsets)
constexpr uint32_t kTva = kScratch, kTvaBytes = 0x100, kEnv = kScratch + 0x200;
constexpr uint32_t kIn = kScratch + 0x400, kFrame = kScratch + 0x800, kPair = kScratch + 0x1000, kFrames = 32;
constexpr uint32_t kFrameStride = 32, kPairStride = 8;   // rom.2's frame of eight buses, rom.4's pair
constexpr int kCases = 2000, kBlocks = 40, kMoveEvery = 10;
// [FW] the object's words by rom.2's offset (HARDWARE_FIRMWARE.md, "The patch TVA's level and pan"), as indexes
constexpr size_t kBlockBus = 0x20 / 4, kSteps = 0x58 / 4, kTargets = 0x60 / 4, kGains = 0x68 / 4, kBus = 0x70 / 4;
constexpr size_t kEnvPtr = 0x94 / 4, kBuses = 8, kChannels = 2;
constexpr uint32_t kGainWords = 0x60, kGainWordsBytes = 0x10;   // targets, gains
constexpr uint32_t kReleased = 5, kSounding = 2;   // [FW] the envelope's state the end test waits for, and one before it
// [FW] where rom.2's render keeps 1/32.5, the ramp's last step to its target (rom.4's at 0x893b2844), and rom.4's value
constexpr uint32_t kRampConstant = 0x894f5844, kRom4Ramp = 0x3cfc0fc1;

using Words = std::array<uint32_t, kTvaBytes / 4>;

uint32_t bitsOf(float x) {
    return std::bit_cast<uint32_t>(x);
}

// A gain or target: at rest near 0 one time in four (where the end test's threshold is), else within ±1
float pickGain(Rng& r) {
    return r.next() % 4 == 0 ? r.in(-1e-6f, 1e-6f) : r.in(-1.0f, 1.0f);
}

// The words into each image's object, rom.4's 0x10 further on
void writeWords(Twin& t, uint32_t offset, const void* words, uint32_t bytes) {
    t.vsynth.write(kTva + offset, words, bytes);
    t.vc2.cpu.write(kTva + kShift + offset, words, bytes);
}

// A step toward the target, as an update forms one: a share of the way each sample
float stepToward(Rng& r, float target, float gain) {
    return (target - gain) * r.in(0.0005f, 0.05f);
}

// The state a note-on and the updates leave, the MAIN pair's levels at 1, and the envelope's state in both
void writeTva(Twin& t, Rng& r) {
    Words w{};
    for (size_t k = 0; k < kBuses; ++k) w[kBlockBus + k] = bitsOf(k < kChannels ? 1.0f : r.in(0.0f, 1.0f));
    for (size_t k = 0; k < kBuses; ++k) w[kBus + k] = w[kBlockBus + k];
    for (size_t c = 0; c < kChannels; ++c) {
        const float target = pickGain(r), gain = pickGain(r);
        w[kTargets + c] = bitsOf(target);
        w[kGains + c] = bitsOf(gain);
        w[kSteps + c] = bitsOf(stepToward(r, target, gain));
    }
    w[kEnvPtr] = kEnv;
    const std::vector<uint8_t> zero(kTvaBytes + kShift, 0);
    t.write(kTva, zero.data(), zero.size());
    writeWords(t, 0, w.data(), sizeof w);
    const uint32_t state = r.next() & 1 ? kReleased : kSounding;
    t.write(kEnv, &state, 4);
}

// What an update moves: the steps and the targets (0x894f5360, "the ramp as in v46, a step per channel")
void moveTargets(Twin& t, Rng& r) {
    std::array<float, kChannels * 2> stepsAndTargets{};
    for (size_t c = 0; c < kChannels; ++c) {
        const float target = pickGain(r);
        const float gain = std::bit_cast<float>(t.vsynth.rd32(kTva + 4 * (kGains + c)));
        stepsAndTargets[kChannels + c] = target;
        stepsAndTargets[c] = stepToward(r, target, gain);
    }
    writeWords(t, 4 * kSteps, stepsAndTargets.data(), sizeof stepsAndTargets);
}

// The largest gap between the two images' gains
float gainGap(Twin& t) {
    float gap = 0;
    for (size_t c = 0; c < kChannels; ++c) {
        const uint32_t at = kTva + 4 * (kGains + c);
        const float g2 = std::bit_cast<float>(t.vsynth.rd32(at));
        const float g4 = std::bit_cast<float>(t.vc2.cpu.rd32(at + kShift));
        gap = std::max(gap, std::fabs(g2 - g4));
    }
    return gap;
}

// A block's signal, and what the other voices have put in the pair so far: rom.2's MAIN pair, rom.4's pair
void writeBlock(Twin& t, Rng& r) {
    std::array<float, kFrames * 2> in{}, sum{};
    const float level = pickLevel(r);
    for (float& x : in) x = r.in(-level, level);
    for (float& x : sum) x = r.in(-level, level);
    t.write(kIn, in.data(), sizeof in);
    for (uint32_t i = 0; i < kFrames; ++i) {
        t.vsynth.write(kFrame + kFrameStride * i, &sum[2 * i], kPairStride);
        t.vc2.cpu.write(kPair + kPairStride * i, &sum[2 * i], kPairStride);
    }
}

// The pair and the gains' words alike in both
bool sameRender(Twin& t) {
    for (uint32_t i = 0; i < kFrames; ++i)
        if (t.differsAt(kFrame + kFrameStride * i, kPair + kPairStride * i, kPairStride) >= 0) return false;
    return t.differsAt(kTva + kGainWords, kTva + kShift + kGainWords, kGainWordsBytes) < 0;
}

// Counted in `n`, the first said
void tally(Twin::Returns& n, bool same, const char* what, int b) {
    ++n.cases;
    if (same) return;
    if (n.differ++ == 0) std::printf("%s differs at block %d\n", what, b);
}

struct TvaCounts {
    Twin::Returns renders, ends;
    long ended = 0;
    float gap = 0;   // the largest gap between the images' gains
};

// rom.2's steps into rom.4's object: what rom.4's render wrote back of them undone
void stepsAsRom2(Twin& t) {
    std::array<uint32_t, kChannels> steps{};
    t.vsynth.read(kTva + 4 * kSteps, steps.data(), sizeof steps);
    t.vc2.cpu.write(kTva + kShift + 4 * kSteps, steps.data(), sizeof steps);
}

// One note's blocks, rom.4's steps made rom.2's before each when `rom2Steps`; false on a fault
bool tvaCase(Twin& t, Rng& r, TvaCounts& n, bool rom2Steps) {
    writeTva(t, r);
    for (int b = 0; b < kBlocks; ++b) {
        if (b % kMoveEvery == kMoveEvery - 1) moveTargets(t, r);
        writeBlock(t, r);
        if (rom2Steps) stepsAsRom2(t);
        if (!t.callVSynth(kRender2, {kTva, kIn, kFrame}) || !t.vc2.run(kRender4, {kTva, kIn, kPair})) return false;
        tally(n.renders, sameRender(t), "the render", b);
        n.gap = std::max(n.gap, gainGap(t));
        if (!t.call(kDone, {kTva})) return false;
        tally(n.ends, t.vsynth.r[0] == t.vc2.cpu.r[0], "the end test", b);
        n.ended += t.vsynth.r[0] == 1 ? 1 : 0;
    }
    return true;
}

// The cases once through, the same each time; their differences, or −1 on a fault
long tvaPass(Twin& t, const char* label, bool rom2Steps = false) {
    Rng r;
    TvaCounts n;
    for (int c = 0; c < kCases; ++c)
        if (!tvaCase(t, r, n, rom2Steps)) return -1;
    std::printf("%s\n", label);
    Twin::Returns all;
    addCount("render, the MAIN pair at level 1", "blocks", n.renders, all);
    addCount("end test", "calls", n.ends, all);
    std::printf("the end test said ended %ld times; the gains at most %.3g apart\n", n.ended, (double)n.gap);
    return sayCount("tva", "calls", all);
}

int tvaCases(Twin& t) {
    const long asIs = tvaPass(t, "as the images are:");
    if (asIs < 0) return -1;
    const uint32_t was = t.vsynth.rd32(kRampConstant);
    t.vsynth.wr32(kRampConstant, kRom4Ramp);
    const long patched = tvaPass(t, "rom.2's 1/32.5 as rom.4's:");
    const long steps = tvaPass(t, "that, and rom.4's steps made rom.2's before each block:", true);
    t.vsynth.wr32(kRampConstant, was);
    return patched < 0 || steps < 0 ? -1 : (int)asIs;
}

} // namespace

int tva(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, noPortTables, tvaCases);
}

} // namespace vc2
