// HIRESO's synthesis: see Vc2Hireso.h.  rom.4's slot 2 (mono, 0x893f0c60, v217), with its carrier bank 0x893f1a10 and
// its groups' synthesis 0x893f1b10 (which slot 2 runs for group 2 and has inline for groups 1 and 0, the same
// arithmetic); and slot 3 (stereo, 0x893f1240, v218), with 0x893f1d40 and 0x893f1ec0, each the mono one on each
// channel: its samples and states interleaved, left then right, the left on the left side's words and the right on the
// right's.  Each routine's arithmetic in its own order, in XtFloat (FMAC a product then a sum, each rounded).
#include "Vc2Hireso.h"
#include "XtFloat.h"
#include <algorithm>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;
using Tables = Vc2HiresoTables;
using Words = Vc2HiresoWords;

constexpr int kMaxChannels = 2;
} // namespace

void Vc2Hireso::synthesis(float* frames, int channels) {
    float full[kMaxChannels * Vc2HiresoAnalysis::kBlock], half[kMaxChannels * Vc2HiresoAnalysis::kHalf];
    float quarter[kMaxChannels * Vc2HiresoAnalysis::kQuarter];
    // mono: each frame's two words × 0.5, then 32 → 16; stereo: the frames as they are, then 32 → 16 pairs
    vc2DecimateFrames(rates[kToHalf], frames, full, half, channels);
    vc2Decimate(rates[kToQuarter], half, quarter, Vc2HiresoAnalysis::kQuarter, channels);
    const float* const carriers[Words::kGroups] = { quarter, half, full };
    for (int g = Words::kGroups - 1; g >= 0; --g) carrierBank(carriers[g], g, channels);
    analysis.envelopes();
    holdGlide();
    float fullOut[kMaxChannels * Vc2HiresoAnalysis::kBlock], halfOut[kMaxChannels * Vc2HiresoAnalysis::kHalf];
    float quarterOut[kMaxChannels * Vc2HiresoAnalysis::kQuarter];
    float* const outs[Words::kGroups] = { quarterOut, halfOut, fullOut };
    for (int g = Words::kGroups - 1; g >= 0; --g) synthesize(g, outs[g], channels);
    vc2InterpolateAdd(rates[kFromQuarter], quarterOut, halfOut, channels);
    vc2InterpolateFrames(rates[kFromHalf], halfOut, fullOut, frames, channels);   // mono: each frame's first word
}

void Vc2Hireso::carrierBank(const float* in, int group, int channels) {
    const Tables& tb = vc2HiresoTables();
    const int first = s.edge[group], end = s.edge[group + 1];
    float* base = &s.bank[Words::kBankWords * first];
    // the group's coefficients where its word points: its lower edge's, then each band's centre's and upper edge's
    const float* coefs = tb.carrier + (s.carrierTable[group] - tb.carrierAt) / 4;
    const int stride = channels * Tables::kStates;   // a resonator's states, its channels' interleaved
    for (int n = 0; n < tb.samples[group]; ++n)
        for (int x = 0; x < channels; ++x) {
            const float v = in[channels * n + x];
            const float d = sub(v, base[x]);   // the first difference
            base[x] = v;
            const float* c = coefs;
            float* st = base + channels + x;
            float lower = vc2HiresoResonate(c, st, d, channels);
            c += Tables::kCoefs;
            st += stride;
            for (int b = first; b < end; ++b, c += 2 * Tables::kCoefs, st += 2 * stride) {
                const float centre = vc2HiresoResonate(c, st, d, channels);
                const float upper = vc2HiresoResonate(c + Tables::kCoefs, st + stride, d, channels);
                s.rows[b][channels * n + x] = add(add(lower, centre), upper);
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

void Vc2Hireso::synthesize(int group, float* out, int channels) {
    const Tables& tb = vc2HiresoTables();
    const int n = tb.samples[group];
    const float perSample = xtf::div(1.0f, xtf::flt(n));
    const float scale = tb.rateScale[group];
    std::fill_n(out, channels * n, 0.0f);
    for (int b = s.edge[group]; b < s.edge[group + 1]; ++b)
        for (int x = 0; x < channels; ++x) {   // channel x on side x's words
            Vc2HiresoSideWords& side = s.*kVc2HiresoSides[x];
            const float e = envelopeAt((s.*kVc2HiresoMaps[x])[b]);
            float& held = side.held[b];
            float& follower = side.follower[b];
            float& target = side.target[b];
            held = add(mul(t.hold.keep, held), mul(t.hold.take, e));
            const float step = mul(sub(held, target), perSample);   // the ramp from the last target to this one
            const float coef = mul(target > follower ? t.attack : t.release[b], scale);
            const float gain = mul(gains[x][b], s.weight[b]);
            float ramp = target, f = follower;
            for (int i = 0; i < n; ++i) {
                const float gap = sub(ramp, f);
                ramp = add(ramp, step);
                f = add(mul(gap, coef), f);
                float& o = out[channels * i + x];
                o = add(mul(mul(s.rows[b][channels * i + x], f), gain), o);
            }
            follower = f;
            target = ramp;
        }
}
