// The VC-2's COSM TVF's converter: see Vc2CosmTvf.h (v235).  In rom.4's order of operations, single precision under
// the SH-4's rounding (XtFloat).
#include "Vc2CosmTvf.h"
#include "Vc2CosmTables.h"
#include "XtFloat.h"

using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;

namespace {
constexpr int32_t kCode = 13;                                        // [FW] its block code
constexpr int kMost = 127, kKeyCentre = 60, kFollowScale = 10;       // [FW] the holds, the keyfollow's key and ×10
// [FW] its parameters in the input, and the Matrix sums each takes (the COSM group's 1st, 10th, 2nd and 11th)
constexpr int kType = 0, kQ = 1, kQLfo = 2, kQEnv = 3, kCutoff = 4, kFollow = 5, kCutoffLfo = 6, kCutoffEnv = 7;
constexpr int kCutoffSum = 0, kCutoffLfoSum = 9, kQSum = 1, kQLfoSum = 10;
// [FW] the envelopes the binding takes, COSM 1's and COSM 2's
constexpr int32_t kCutoffEnvelope[2] = { 9, 11 }, kQEnvelope[2] = { 10, 12 };
} // namespace

void vc2TvfConvert(const Vc2CosmInput& in, Vc2CosmTvfBlock& b, Vc2CosmMatrix& matrix) {
    const Vc2CosmTvfPool& k = vc2CosmTables().tvf;
    const int second = in.second != 0 ? 1 : 0;
    const auto sum = [&](int index) { return matrix.sums(second != 0, in.matrix)[index]; };
    b.code = kCode;
    b.filterType = in.params[kType];
    // the Cutoff and its keyfollow, (key − 60) × KF × 10 per cent per step
    const int cutoff = vc2CosmLevel(in.params[kCutoff], sum(kCutoffSum), kMost);
    const auto key = static_cast<int16_t>(in.noteKey - kKeyCentre);
    const auto follow = static_cast<int16_t>(in.params[kFollow] * kFollowScale);
    const float keyed = div(div(mul(flt(key), flt(follow)), k.percent), k.k127);
    const float level = add(add(keyed, div(flt(cutoff), flt(kMost))), 0.0f);
    b.cutoff = level > 1.0f ? 1.0f : 0.0f > level ? 0.0f : level;
    b.cutoffMod = vc2CosmModOf(in.params[kCutoffEnv], in.velocity[0], kCutoffEnvelope[second], in.params[kCutoffLfo],
                               sum(kCutoffLfoSum), k.k63);
    const int q = vc2CosmLevel(in.params[kQ], sum(kQSum), kMost);
    b.q = add(div(mul(flt(q), k.qScale), flt(kMost)), -1.0f);
    b.qMod = vc2CosmModOf(in.params[kQEnv], in.velocity[1], kQEnvelope[second], in.params[kQLfo], sum(kQLfoSum), k.k63);
}
