// The VC-2 part's conversion sets' TVA: see Vc2SetTva.h (v285).  The writer's reads, calls and writes in rom.4's order;
// the floats through XtFloat, as the SH-4 rounds.
#include "Vc2SetTva.h"
#include "Vc2LibMath.h"
#include "Vc2SetEnvelopeTables.h"
#include "XtFloat.h"
#include <algorithm>

namespace {
constexpr uint32_t kCode = 13;                          // [FW] the block's +0
constexpr int32_t kTop = 127, kCentreKey = 60, kCentre = 64;
constexpr int32_t kDepthMost = 63;                      // [FW] the depths' hold
constexpr int32_t kFollowMagic = 0x057619f1;            // [FW] rom.4's reciprocal: (127 × n) / 6000 by its shift
constexpr int kFollowShift = 7, kFollowTimes = 7;       // [FW] and n × 127 as (n << 7) − n
constexpr int32_t kPanMagic = static_cast<int32_t>(0x82082083u);   // [FW] n / 63 by its shift
constexpr int kPanShift = 5, kPanLowShift = 6;          // [FW] and n / 64 below the centre
constexpr int32_t kCrossfadeAt = 128;                   // [FW] the crossfade's table 0x89542034 in the level run
constexpr int32_t kOffLeft = 63, kOffRight = 64;        // [FW] off, the crossfade's middle pair
constexpr uint32_t kReader = 0x893a2bf0;                // [FW] the group whose sums the TVA takes
constexpr uint32_t kLevelSum = 0xc, kDepthASum = 8, kDepthBSum = 0xa, kPanSum = 0x14;   // [FW] the sums' offsets
constexpr uint32_t kOutputSum = 0xe, kSendASum = 0x12, kSendBSum = 0x10;
constexpr uint32_t kCc7 = 0, kCc11 = 1, kCc10 = 2, kCc93 = 3;   // [FW] M's controllers from +0x178
constexpr float kOne = 1.0f, kZero = 0.0f, kTwo = 2.0f;         // fldi1, fldi0, fldi1 + fadd

const Vc2SetEnvelopeTables& tvaTables() { return vc2SetEnvelopeTables(); }

// A key follow: (key − 60) × by × 127 in 32 bits, by rom.4's /6000
int32_t tvaFollowed(uint8_t key, int16_t by) {
    const uint32_t n = static_cast<uint32_t>(static_cast<int32_t>(key) - kCentreKey) * static_cast<uint32_t>(by);
    return vc2ByMagic(static_cast<int32_t>((n << kFollowTimes) - n), kFollowMagic, kFollowShift);
}

// A byte plus its sum in the group's answer
int32_t summedByte(const Vc2SetTvaWords& w, uint32_t offset, int32_t byte, Vc2SetReads& reads) {
    return reads.matrixSum(kReader, w.matrix, offset) + byte;
}

int32_t tvaHeld(int32_t v) { return std::clamp(v, 0, kTop); }

// A depth: held to ±63, then sign · (d / 63)²
float signedDepth(int32_t d) {
    const int32_t at = std::clamp(d, -kDepthMost, kDepthMost);
    const float x = xtf::div(xtf::flt(at), tvaTables().tvaDepthTop);
    return xtf::mul(at < 0 ? -x : x, x);
}

// CC 10 on the pan: past the centre toward 127 by n / 63, below it toward 0 by n / 64 (p ≥ 0: rom.4's rounding of a
// negative never applies)
int32_t tvaPanned(int32_t p, uint8_t cc) {
    if (cc >= kCentre) return p + vc2ByMagic((kTop - p) * (cc - kCentre), kPanMagic, kPanShift);
    return p - ((p * (kCentre - cc)) >> kPanLowShift);
}

// On (0x893a6296…): the level by its sum, key follow, the part's CC 7 and 11 and the curve's level; the two depths;
// the pan by its sum, key follow and CC 10, its pair into `pair`
void onWay(const Vc2SetTvaWords& w, uint32_t* block, Vc2SetReads& reads, float scale, float* pair) {
    const int32_t sum = tvaHeld(summedByte(w, kLevelSum, w.level, reads));
    float level = vc2LevelRun(tvaHeld(static_cast<int16_t>(tvaFollowed(w.key, w.levelFollow) + sum)), scale);
    if (reads.tvaGlobals().controls != 0) {
        const float cc7 = vc2LevelRun(reads.matrixControl(w.matrix, kCc7), scale);
        level = xtf::mul(xtf::mul(cc7, level), vc2LevelRun(reads.matrixControl(w.matrix, kCc11), scale));
    }
    block[3] = floatWord(xtf::mul(level, w.velocityLevel));
    block[2] = floatWord(signedDepth(summedByte(w, kDepthASum, w.depthA, reads)));
    block[6] = floatWord(kZero);
    block[7] = floatWord(signedDepth(summedByte(w, kDepthBSum, w.depthB, reads)));
    const int32_t pan = static_cast<int16_t>(summedByte(w, kPanSum, w.pan, reads));
    const int32_t p = tvaPanned(tvaHeld(static_cast<int16_t>(tvaFollowed(w.key, w.panFollow) + pan)),
                                reads.matrixControl(w.matrix, kCc10));
    pair[0] = vc2LevelRun(kCrossfadeAt + kTop - p, scale);
    pair[1] = vc2LevelRun(kCrossfadeAt + p, scale);
}

// Off (0x893a6500): 1.0 for the level, no depths, the crossfade's middle pair
void offWay(uint32_t* block, float scale, float* pair) {
    block[3] = floatWord(kOne);
    block[2] = floatWord(kZero);
    block[6] = floatWord(kZero);
    block[7] = floatWord(kZero);
    pair[0] = vc2LevelRun(kCrossfadeAt + kOffLeft, scale);
    pair[1] = vc2LevelRun(kCrossfadeAt + kOffRight, scale);
}

void tvaPairs(uint32_t* block, float first, float second) {
    block[8] = block[9] = floatWord(first);
    block[10] = block[11] = floatWord(second);
}

// [FW] the outputs by the route byte (the writer's table 0x893a6608): 0 the second pair, 1 the first, 2 the first
// unless the part's flag is set; past 2 as 1
using Route = void (*)(uint32_t*, float, const Vc2SetTvaGlobals&);
void routeSecond(uint32_t* block, float out, const Vc2SetTvaGlobals&) { tvaPairs(block, kZero, out); }
void routeFirst(uint32_t* block, float out, const Vc2SetTvaGlobals&) { tvaPairs(block, out, kZero); }
void routeByPart(uint32_t* block, float out, const Vc2SetTvaGlobals& g) {
    tvaPairs(block, g.route == 0 ? out : kZero, kZero);
}
constexpr std::array<Route, 3> kRoutes { routeSecond, routeFirst, routeByPart };

void tvaScaled(uint32_t& word, float by) { word = floatWord(xtf::mul(std::bit_cast<float>(word), by)); }

// The velocity curve's level, squared
void velocityLevelOf(Vc2SetTvaWords& w, Vc2SetReads& reads) {
    const int r = reads.velocityCurve(w.velocity, w.sens, w.curve);
    const float x = xtf::div(xtf::flt(static_cast<uint16_t>(r)), tvaTables().tvaVelocity);
    w.velocityLevel = xtf::mul(x, x);
}
} // namespace

void Vc2SetTvas::blockOf(const Vc2SetTvaWords& w, uint32_t* block, Vc2SetReads& reads) {
    const Vc2SetEnvelopeTables& t = tvaTables();
    block[0] = kCode;
    block[1] = 0;
    block[4] = 0;
    block[5] = 0;
    const float scale = w.on != 0 ? t.tvaScale : t.tvaScaleOff;
    float pair[2];
    if (w.on != 0)
        onWay(w, block, reads, scale, pair);
    else
        offWay(block, scale, pair);
    const float out = vc2LevelRun(tvaHeld(summedByte(w, kOutputSum, w.output, reads)), scale);
    block[18] = floatWord(pair[0]);
    block[19] = floatWord(pair[1]);
    block[16] = floatWord(w.kind == 0 ? kOne : kZero);
    block[17] = floatWord(w.kind == 0 ? kZero : kOne);
    const Vc2SetTvaGlobals g = reads.tvaGlobals();
    (w.route < kRoutes.size() ? kRoutes[w.route] : routeFirst)(block, out, g);
    block[12] = floatWord(xtf::mul(vc2LevelRun(tvaHeld(summedByte(w, kSendASum, w.sendA, reads)), scale), t.tvaSend));
    const float sendB = vc2LevelRun(tvaHeld(summedByte(w, kSendBSum, w.sendB, reads)), scale);
    block[13] = floatWord(xtf::mul(vc2LevelRun(reads.matrixControl(w.matrix, kCc93), scale), sendB));
    const float gain = vc2LibPow(kTwo, xtf::div(xtf::flt(g.gain), t.tvaSixths));
    tvaScaled(block[8], gain);
    tvaScaled(block[9], gain);
    const float common = xtf::mul(vc2LevelRun(w.gain, scale), gain);
    block[12] = floatWord(xtf::mul(common, std::bit_cast<float>(block[12])));
    tvaScaled(block[13], gain);
    tvaScaled(block[10], gain);
    tvaScaled(block[11], gain);
    block[14] = floatWord(xtf::mul(common, t.tvaSendTwo));
    block[15] = floatWord(xtf::mul(gain, out));
    step = xtf::div(kOne, xtf::mul(xtf::div(xtf::flt(g.rate), t.tvaRateScale), t.tvaSampleRate));
}

void Vc2SetTvas::construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads) {
    Vc2SetTvaWords& w = madeByKind(c, reads);
    velocityLevelOf(w, reads);
    blockOf(w, block, reads);
}

void Vc2SetTvas::convert(uint32_t tva, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads) {
    Vc2SetTvaWords& w = at(tva);
    vc2ChildVoice(&w.key, call, reads);
    velocityLevelOf(w, reads);
    blockOf(w, block, reads);
}
