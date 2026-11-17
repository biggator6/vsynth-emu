// The VC-2's COSM Dual Filter's and TB Filter's converters: see Vc2CosmFilters.h (v237).  In rom.4's order of
// operations, single precision under the SH-4's rounding (XtFloat).
#include "Vc2CosmFilters.h"
#include "Vc2CosmTables.h"
#include "XtFloat.h"

using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;

namespace {
constexpr int kMost = 127;                                  // [FW] the levels' hold
// [FW] the Dual Filter's: each filter's Frequency, Keyfollow, Q, LFO and Env depths in the input, its two sums (the
// group's 1st and 10th, 2nd and 11th), its envelopes (COSM 1's, COSM 2's) and its velocity factor
struct DualRow {
    int frequency, follow, q, lfo, env, sum, lfoSum;
    int32_t envelope[2];
    int velocity;
};
constexpr DualRow kDual[2] { { 1, 2, 3, 4, 5, 0, 9, { 9, 11 }, 0 }, { 6, 7, 8, 9, 10, 1, 10, { 10, 12 }, 1 } };
constexpr int32_t kDualCode = 12, kTbCode = 20;            // [FW]
// [FW] the TB Filter's parameters: LPF Cutoff, its Keyfollow, LFO and Env depths, Q, Q Follow; HPF Cutoff, its
// Keyfollow, LFO and Env depths; Gain
constexpr int kLpf = 0, kLpfFollow = 1, kLpfLfo = 2, kLpfEnv = 3, kQ = 4, kQFollow = 5, kHpf = 6, kHpfFollow = 7,
              kHpfLfo = 8, kHpfEnv = 9, kGain = 10;
constexpr int kLpfSum = 0, kLpfLfoSum = 9, kQSum = 1;      // [FW] its sums
constexpr int32_t kLpfEnvelope[2] = { 9, 11 }, kHpfEnvelope[2] = { 10, 12 };
constexpr int kFollowMost = 200, kFollowSpan = 400;        // [FW] a keyfollow's KF·10 hold and span
constexpr int kQFollowMost = 63, kQFollowSpan = 126;       // [FW] Q Follow's

// A keyfollow as the TB Filter's: (KF·10 held to ±200, + 200) × scale ÷ 400 + from
float tbFollow(int16_t follow, float scale, float from) {
    const auto f = static_cast<int16_t>(follow * 10);
    const int held = f > kFollowMost ? kFollowMost : f < -kFollowMost ? -kFollowMost : f;
    return add(div(mul(flt(held + kFollowMost), scale), flt(kFollowSpan)), from);
}
} // namespace

void vc2DualConvert(const Vc2CosmInput& in, Vc2CosmDualBlock& b, Vc2CosmMatrix& matrix) {
    const Vc2CosmDualPool& k = vc2CosmTables().dual;
    const Vc2CosmSums sum { in, matrix };
    b.code = kDualCode;
    b.filterType = in.params[0];
    for (int f = 0; f < 2; ++f) {
        const DualRow& r = kDual[f];
        Vc2CosmDualSide& s = b.filter[f];
        const int frequency = vc2CosmLevel(in.params[r.frequency], sum(r.sum), kMost);
        s.frequency = vc2CosmKeyed(frequency, in.noteKey, in.params[r.follow], k.percent, k.k127);
        s.q = vc2CosmQ(vc2CosmLevel(in.params[r.q], 0, kMost), k.qScale);
        const Vc2CosmMod m = vc2CosmModOf(in.params[r.env], in.velocity[r.velocity], r.envelope[sum.second()],
                                          in.params[r.lfo], sum(r.lfoSum), k.k63);
        s.env = m.env;
        s.envelope = m.envelope;
        s.lfo = m.lfo;
        s.lfoNumber = m.lfoNumber;
    }
}

void vc2TbConvert(const Vc2CosmInput& in, Vc2CosmTbBlock& b, Vc2CosmMatrix& matrix) {
    const Vc2CosmTbPool& k = vc2CosmTables().tb;
    const Vc2CosmSums sum { in, matrix };
    b.code = kTbCode;
    b.lpf = vc2CosmUnit(flt(vc2CosmLevel(in.params[kLpf], sum(kLpfSum), kMost)));
    b.lpfFollow = tbFollow(in.params[kLpfFollow], k.lpfFollow, k.lpfFollowFrom);
    b.lpfMod = vc2CosmModOf(in.params[kLpfEnv], in.velocity[0], kLpfEnvelope[sum.second()], in.params[kLpfLfo],
                            sum(kLpfLfoSum), k.k63);
    b.lpfQ = vc2CosmUnit(flt(vc2CosmLevel(in.params[kQ], sum(kQSum), kMost)));
    const int16_t qf = in.params[kQFollow];
    const int heldQf = qf > kQFollowMost ? kQFollowMost : qf < -kQFollowMost ? -kQFollowMost : qf;
    b.qFollow = add(div(mul(flt(heldQf + kQFollowMost), 2.0f), flt(kQFollowSpan)), -1.0f);
    b.hpf = vc2CosmUnit(flt(vc2CosmLevel(in.params[kHpf], 0, kMost)));
    b.hpfFollow = tbFollow(in.params[kHpfFollow], k.hpfFollow, k.hpfFollowFrom);
    b.hpfMod = { mul(in.velocity[1], vc2CosmDepth(in.params[kHpfEnv], k.k63)), kHpfEnvelope[sum.second()],
                 vc2CosmDepth(in.params[kHpfLfo], k.k63), 0 };
    b.gain = mul(vc2CosmGain(in.params[kGain], k.gainScale), 2.0f);
    b.key = in.noteKey;
}
