// The VC-2's COSM Side Band Filters' converters: see Vc2CosmSideBand.h (v241).  In rom.4's order of operations, single
// precision under the SH-4's rounding (XtFloat).
#include "Vc2CosmSideBand.h"
#include "Vc2CosmTables.h"
#include "Vc2VocoderOsc.h"
#include "XtFloat.h"

using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;
using xtf::sub;

namespace {
constexpr int kMost = 127, kDetuneMost = 100, kDetuneSpan = 200;   // [FW] the levels' hold; Detune's and its sum's
constexpr int32_t kSbf1Code = 7, kSbf2Code = 8;                    // [FW]
constexpr int32_t kWidthEnvelope[2] = { 9, 11 }, kDetuneEnvelope[2] = { 10, 12 };   // [FW] COSM 1's, COSM 2's
// [FW] the sums: the group's 1st, 10th, 2nd and 11th
constexpr int kWidthSum = 0, kWidthLfoSum = 9, kDetuneSum = 1, kDetuneLfoSum = 10;
constexpr int kLpf = 1, kLpfFollow = 2, kHpf = 0, kSubDetune = 3, kQ = 0;   // [FW] the converters' own parameters

// [FW] a Side Band Filter's parameters in its input: Octave Select, Detune and its LFO, Own Env and Env depths, Width,
// its Keyfollow and its LFO, Own Env and Env depths; its pool
struct Vc2CosmSbfRow {
    int octave, detune, detuneLfo, detuneOwn, detuneEnv, width, widthFollow, widthLfo, widthOwn, widthEnv;
    Vc2CosmSideBandPool Vc2CosmTables::*pool;
};
constexpr Vc2CosmSbfRow kSbf1 { 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, &Vc2CosmTables::sbf1 };
constexpr Vc2CosmSbfRow kSbf2 { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, &Vc2CosmTables::sbf2 };

// A detune's hold, ±100, on 16 bits
int heldDetune(int16_t d) { return d > kDetuneMost ? kDetuneMost : d < -kDetuneMost ? -kDetuneMost : d; }

// Detune and its sum × 200/127 (an integer's, toward 0), held to 16 bits, then to ±100
int detuneOf(int16_t detune, int16_t sum) {
    return heldDetune(static_cast<int16_t>(detune + sum * kDetuneSpan / kMost));
}

// 0x893975a0, the LPF: Cutoff held to 0…127 × ½ ÷ 127 + ½, plus or (x below 0) less ln 2 × x² × ¼, x the keyfollow's
// offset ÷ 100 ÷ 12, x² and ln 2 rom.4's powf(x, 2) and logf(2); held to ½…1
float lpfOf(int16_t lpf, uint8_t key, int16_t follow, const Vc2CosmSideBandLaws& k, const Vc2ToneShared& shared) {
    const float base = add(div(mul(flt(vc2CosmLevel(lpf, 0, kMost)), k.half), flt(kMost)), k.half);
    const float x = vc2CosmFollowed(key, follow, k.percent, k.twelve);
    const float d = mul(mul(shared.pow(x, 2.0f), shared.log(2.0f)), k.quarter);
    const float y = 0.0f > x ? sub(base, d) : add(base, d);
    return y > 1.0f ? 1.0f : k.half > y ? k.half : y;
}

// Width, then Detune, each with its depths, the sums in rom.4's order
void sideBandBody(const Vc2CosmSbfRow& r, const Vc2CosmInput& in, Vc2CosmSbfBody& b, Vc2CosmMatrix& matrix,
                  const Vc2ToneShared& shared) {
    const Vc2CosmSideBandPool& k = vc2CosmTables().*r.pool;
    const Vc2CosmSums sum { in, matrix };
    const int width = vc2CosmLevel(in.params[r.width], sum(kWidthSum), kMost);
    b.width.value = vc2CosmScaled(width, in.noteKey, in.params[r.widthFollow], vc2CosmTables().sideBand.widthCents,
                                  shared);
    b.width.mod = vc2CosmModOf(in.params[r.widthEnv], in.velocity[0], kWidthEnvelope[sum.second()],
                               in.params[r.widthLfo], sum(kWidthLfoSum), k.k63);
    b.width.own = vc2CosmDepth(in.params[r.widthOwn], k.k63);
    b.detune.value = flt(detuneOf(in.params[r.detune], sum(kDetuneSum)));
    b.detune.mod = vc2CosmModOf(in.params[r.detuneEnv], in.velocity[1], kDetuneEnvelope[sum.second()],
                                in.params[r.detuneLfo], sum(kDetuneLfoSum), k.k63);
    b.detune.own = vc2CosmDepth(in.params[r.detuneOwn], k.ownK63);
}

float lpfOf(const Vc2CosmInput& in, const Vc2ToneShared& shared) {
    return lpfOf(in.params[kLpf], in.noteKey, in.params[kLpfFollow], vc2CosmTables().sideBand, shared);
}
} // namespace

void vc2Sbf1Convert(const Vc2CosmInput& in, Vc2CosmSbf1Block& b, Vc2CosmMatrix& matrix, const Vc2ToneShared& shared) {
    b.code = kSbf1Code;
    b.hpf = add(div(-flt(vc2CosmLevel(in.params[kHpf], 0, kMost)), flt(kMost)), 0.0f);
    b.lpf = lpfOf(in, shared);
    b.subDetune = flt(heldDetune(in.params[kSubDetune]));
    b.octave = static_cast<uint8_t>(in.params[kSbf1.octave]);
    sideBandBody(kSbf1, in, b.body, matrix, shared);
    b.one = 1.0f;
}

void vc2Sbf2Convert(const Vc2CosmInput& in, Vc2CosmSbf2Block& b, Vc2CosmMatrix& matrix, const Vc2ToneShared& shared) {
    b.code = kSbf2Code;
    b.q = vc2CosmQ(vc2CosmLevel(in.params[kQ], 0, kMost), vc2CosmTables().sbf2.qScale);
    b.lpf = lpfOf(in, shared);
    b.octave = static_cast<uint8_t>(in.params[kSbf2.octave]);
    sideBandBody(kSbf2, in, b.body, matrix, shared);
    b.one = 1.0f;
}
