// HIRESO's mono synthesis (v217): see Vc2Hireso.h.  rom.4's slot 2, 0x893f0c60, with its carrier bank 0x893f1a10 and
// its groups' synthesis 0x893f1b10 (which slot 2 runs for group 2 and has inline for groups 1 and 0, the same
// arithmetic).  Each routine's arithmetic in its own order, in XtFloat (FMAC a product then a sum, each rounded).
#include "Vc2Hireso.h"
#include "XtFloat.h"
#include <algorithm>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;
using Tables = Vc2HiresoTables;
using Words = Vc2HiresoWords;

constexpr int kLeft = 0;   // mono's side: the left's map, template and band words
} // namespace

void Vc2Hireso::mono(float* frames) {
    float mixed[Vc2HiresoAnalysis::kBlock], half[Vc2HiresoAnalysis::kHalf], quarter[Vc2HiresoAnalysis::kQuarter];
    vc2DecimateFrames(rates[kToHalf], frames, mixed, half);   // each frame's two words × 0.5, then 32 → 16
    vc2Decimate(rates[kToQuarter], half, quarter, Vc2HiresoAnalysis::kQuarter);
    const float* const carriers[Words::kGroups] = { quarter, half, mixed };
    for (int g = Words::kGroups - 1; g >= 0; --g) carrierBank(carriers[g], g);
    analysis.envelopes();
    holdGlide();
    float full[Vc2HiresoAnalysis::kBlock], halfOut[Vc2HiresoAnalysis::kHalf], quarterOut[Vc2HiresoAnalysis::kQuarter];
    float* const outs[Words::kGroups] = { quarterOut, halfOut, full };
    for (int g = Words::kGroups - 1; g >= 0; --g) synthesize(g, outs[g]);
    vc2InterpolateAdd(rates[kFromQuarter], quarterOut, halfOut);
    vc2InterpolateFrames(rates[kFromHalf], halfOut, full, frames);   // into each frame's first word
}

void Vc2Hireso::carrierBank(const float* in, int group) {
    const Tables& tb = vc2HiresoTables();
    const int first = s.edge[group], end = s.edge[group + 1];
    float* base = &s.bank[Words::kBankWords * first];
    // the group's coefficients where its word points: its lower edge's, then each band's centre's and upper edge's
    const float* coefs = tb.carrier + (s.carrierTable[group] - tb.carrierAt) / 4;
    for (int n = 0; n < tb.samples[group]; ++n) {
        const float d = sub(in[n], base[0]);   // the first difference
        base[0] = in[n];
        const float* c = coefs;
        float* st = base + 1;
        float lower = vc2HiresoResonate(c, st, d);
        c += Tables::kCoefs;
        st += Tables::kStates;
        for (int b = first; b < end; ++b, c += 2 * Tables::kCoefs, st += 2 * Tables::kStates) {
            const float centre = vc2HiresoResonate(c, st, d);
            const float upper = vc2HiresoResonate(c + Tables::kCoefs, st + Tables::kStates, d);
            s.rows[b][n] = add(add(lower, centre), upper);
            lower = upper;   // the next band's lower edge
        }
    }
}

float Vc2Hireso::envelopeAt(float band) const {
    const int i = xtf::ftrc(band);
    const float frac = sub(band, xtf::flt(i));
    if (i >= Words::kBands - 1) return analysis.envelope(Words::kBands - 1);
    return add(mul(analysis.envelope(i + 1), frac), mul(analysis.envelope(i), sub(1.0f, frac)));
}

void Vc2Hireso::synthesize(int group, float* out) {
    const Tables& tb = vc2HiresoTables();
    const int n = tb.samples[group];
    const float perSample = xtf::div(1.0f, xtf::flt(n));
    const float scale = tb.rateScale[group];
    std::fill_n(out, n, 0.0f);
    const float* map = s.formant + Words::kSideWords * kLeft;
    for (int b = s.edge[group]; b < s.edge[group + 1]; ++b) {
        const float e = envelopeAt(map[b]);
        float& held = s.bandWords[Words::kHeld][b];
        float& follower = s.bandWords[Words::kFollower][b];
        float& target = s.bandWords[Words::kTarget][b];
        held = add(mul(t.hold.keep, held), mul(t.hold.take, e));
        const float step = mul(sub(held, target), perSample);   // the ramp from the last target to this one
        const float coef = mul(target > follower ? t.attack : t.release[b], scale);
        const float gain = mul(gains[kLeft][b], s.weight[b]);
        float ramp = target, f = follower;
        for (int i = 0; i < n; ++i) {
            const float gap = sub(ramp, f);
            ramp = add(ramp, step);
            f = add(mul(gap, coef), f);
            out[i] = add(mul(mul(s.rows[b][i], f), gain), out[i]);
        }
        follower = f;
        target = ramp;
    }
}
