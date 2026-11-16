// HIRESO's rate filters: see Vc2HiresoRate.h (v214).  Every address is rom.4's; the check is xt_vc2hireso's
// (Vc2HiresoRateCheck.cpp).  Each step is one SH-4 instruction's rounding (XtFloat); an FMAC is the product rounded,
// then the sum.
#include "Vc2HiresoRate.h"
#include "XtFloat.h"
#include <algorithm>

namespace {
constexpr int kBiquads = 3, kOnePole = 6, kDelay = 7;   // the states' places
constexpr float kMix = 0.5f;                            // [FW 0x893efee4] the carrier's two words, mixed

// The three biquads on one sample, as both kinds run them: section k's coefficients a, b, n (after the input's gain)
// on its states s0, s1: w = b·s1 + (a·s0 + in); out = w + (n·s0 + s1)
float sections(const float* c, float* s, float in) {
    using namespace xtf;
    float y = in;
    for (int k = 0; k < kBiquads; ++k) {
        const float* ck = c + 1 + 3 * k;
        float* sk = s + 2 * k;
        const float w = add(mul(ck[1], sk[1]), add(mul(ck[0], sk[0]), y));
        y = add(w, add(mul(ck[2], sk[0]), sk[1]));
        sk[1] = sk[0];
        sk[0] = w;
    }
    return y;
}

// An interpolator's sample: the biquads, then the one-pole on state 6, w = c10·s6 + in; out = w + s6
float interpolated(const float* c, float* s, float in) {
    using namespace xtf;
    const float w = add(mul(c[10], s[kOnePole]), sections(c, s, in));
    const float out = add(s[kOnePole], w);
    s[kOnePole] = w;
    return out;
}
} // namespace

void vc2RateClear(Vc2RateFilter& f) { std::fill(std::begin(f.s), std::end(f.s), 0.0f); }

void vc2Decimate(Vc2RateFilter& f, const float* in, float* out, int outputs) {
    for (int n = 0; n < outputs; ++n) {
        sections(f.c, f.s, xtf::mul(f.c[0], in[2 * n]));
        out[n] = sections(f.c, f.s, xtf::mul(f.c[0], in[2 * n + 1]));
    }
}

void vc2DecimateFrames(Vc2RateFilter& f, const float* frames, float* mixed, float* out) {
    using namespace xtf;
    for (int i = 0; i < 32; ++i) mixed[i] = mul(add(frames[2 * i], frames[2 * i + 1]), kMix);
    vc2Decimate(f, mixed, out, 16);
}

void vc2InterpolateAdd(Vc2RateFilter& f, const float* in, float* out) {
    using namespace xtf;
    for (int n = 0; n < 8; ++n) {
        out[2 * n] = add(out[2 * n], interpolated(f.c, f.s, mul(f.c[0], in[n])));
        out[2 * n + 1] = add(out[2 * n + 1], interpolated(f.c, f.s, 0.0f));
    }
}

void vc2InterpolateFrames(Vc2RateFilter& f, const float* in, const float* full, float* frames) {
    using namespace xtf;
    for (int n = 0; n < 32; ++n) {
        const float y = interpolated(f.c, f.s, n % 2 == 0 ? mul(f.c[0], in[n / 2]) : 0.0f);
        frames[2 * n] = add(y, f.s[kDelay]);
        f.s[kDelay] = full[n];
    }
}
