// The VC-2's COSM TVF's converter and the Dynamic TVF's: see Vc2CosmTvf.h (v235, v237).  In rom.4's order of
// operations, single precision under the SH-4's rounding (XtFloat).
#include "Vc2CosmTvf.h"
#include "Vc2CosmTables.h"

namespace {
constexpr int kMost = 127;   // [FW] the Cutoff's and Q's hold
// [FW] the Matrix sums each takes (the COSM group's 1st, 10th, 2nd and 11th)
constexpr int kCutoffSum = 0, kCutoffLfoSum = 9, kQSum = 1, kQLfoSum = 10;
// [FW] the envelopes the binding takes, COSM 1's and COSM 2's
constexpr int32_t kCutoffEnvelope[2] = { 9, 11 }, kQEnvelope[2] = { 10, 12 };

// [FW] a TVF's block code, pool and parameters in its input: the TVF's eight, the Dynamic TVF's ten, its two Own Env
// depths after each LFO depth (none: −1)
struct Vc2CosmTvfRow {
    int32_t code;
    Vc2CosmTvfPool Vc2CosmTables::*pool;
    int type, q, qLfo, qOwn, qEnv, cutoff, follow, cutoffLfo, cutoffOwn, cutoffEnv;
};
constexpr Vc2CosmTvfRow kTvf { 13, &Vc2CosmTables::tvf, 0, 1, 2, -1, 3, 4, 5, 6, -1, 7 };
constexpr Vc2CosmTvfRow kDtvf { 14, &Vc2CosmTables::dtvf, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

void convert(const Vc2CosmTvfRow& row, const Vc2CosmInput& in, Vc2CosmTvfBlock& b, Vc2CosmMatrix& matrix) {
    const Vc2CosmTvfPool& k = vc2CosmTables().*row.pool;
    const Vc2CosmSums sum { in, matrix };
    const int second = sum.second();
    const auto own = [&](int param, float& out) {
        if (param >= 0) out = vc2CosmDepth(in.params[param], k.k63);
    };
    b.code = row.code;
    b.filterType = in.params[row.type];
    // the Cutoff and its keyfollow, (key − 60) × KF × 10 per cent per step
    const int cutoff = vc2CosmLevel(in.params[row.cutoff], sum(kCutoffSum), kMost);
    b.cutoff = vc2CosmKeyed(cutoff, in.noteKey, in.params[row.follow], k.percent, k.k127);
    b.cutoffMod = vc2CosmModOf(in.params[row.cutoffEnv], in.velocity[0], kCutoffEnvelope[second],
                               in.params[row.cutoffLfo], sum(kCutoffLfoSum), k.k63);
    own(row.cutoffOwn, b.cutoffOwn);
    b.q = vc2CosmQ(vc2CosmLevel(in.params[row.q], sum(kQSum), kMost), k.qScale);
    b.qMod = vc2CosmModOf(in.params[row.qEnv], in.velocity[1], kQEnvelope[second], in.params[row.qLfo], sum(kQLfoSum),
                          k.k63);
    own(row.qOwn, b.qOwn);
}
} // namespace

void vc2TvfConvert(const Vc2CosmInput& in, Vc2CosmTvfBlock& b, Vc2CosmMatrix& matrix) { convert(kTvf, in, b, matrix); }

void vc2DtvfConvert(const Vc2CosmInput& in, Vc2CosmTvfBlock& b, Vc2CosmMatrix& matrix) {
    convert(kDtvf, in, b, matrix);
}
