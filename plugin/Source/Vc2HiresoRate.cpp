// HIRESO's rate filters: see Vc2HiresoRate.h (v214; the stereo path's, v218).  Every address is rom.4's; the check is
// xt_vc2hireso's (Vc2HiresoRateCheck.cpp).  Each step is one SH-4 instruction's rounding (XtFloat); an FMAC is the
// product rounded, then the sum.  A stereo routine is the mono one on each channel: its samples interleaved, and its
// states too, a channel's at every `channels`-th word from its own.
#include "Vc2HiresoRate.h"
#include "XtFloat.h"
#include <algorithm>

namespace {
constexpr int kBiquads = 3, kOnePole = 6, kDelay = 7;   // the states' places, in a channel's words
constexpr int kFull = 32, kHalf = 16, kQuarter = 8;     // a block's samples at each rate
constexpr float kMix = 0.5f;                            // [FW 0x893efee4] the carrier's two words, mixed

// The three biquads on one sample of one channel, as both kinds run them: section k's coefficients a, b, n (after the
// input's gain) on its states s0, s1 (`ch` words apart): w = b·s1 + (a·s0 + in); out = w + (n·s0 + s1)
float sections(const float* c, float* s, float in, int ch) {
    using namespace xtf;
    float y = in;
    for (int k = 0; k < kBiquads; ++k) {
        const float* ck = c + 1 + 3 * k;
        float* sk = s + 2 * k * ch;
        const float w = add(mul(ck[1], sk[ch]), add(mul(ck[0], sk[0]), y));
        y = add(w, add(mul(ck[2], sk[0]), sk[ch]));
        sk[ch] = sk[0];
        sk[0] = w;
    }
    return y;
}

// An interpolator's sample of one channel: the biquads, then the one-pole on state 6, w = c10·s6 + in; out = w + s6
float interpolated(const float* c, float* s, float in, int ch) {
    using namespace xtf;
    const float w = add(mul(c[10], s[kOnePole * ch]), sections(c, s, in, ch));
    const float out = add(s[kOnePole * ch], w);
    s[kOnePole * ch] = w;
    return out;
}
} // namespace

void vc2RateClear(Vc2RateFilter& f) { std::fill(std::begin(f.s), std::end(f.s), 0.0f); }

void vc2Decimate(Vc2RateFilter& f, const float* in, float* out, int outputs, int channels) {
    for (int n = 0; n < outputs; ++n)
        for (int x = 0; x < channels; ++x) {
            sections(f.c, f.s + x, xtf::mul(f.c[0], in[channels * 2 * n + x]), channels);
            out[channels * n + x] = sections(f.c, f.s + x, xtf::mul(f.c[0], in[channels * (2 * n + 1) + x]), channels);
        }
}

void vc2DecimateFrames(Vc2RateFilter& f, const float* frames, float* full, float* out, int channels) {
    using namespace xtf;
    if (channels == 1) {
        for (int i = 0; i < kFull; ++i) full[i] = mul(add(frames[2 * i], frames[2 * i + 1]), kMix);
    } else {
        std::copy(frames, frames + 2 * kFull, full);
    }
    vc2Decimate(f, full, out, kHalf, channels);
}

void vc2InterpolateAdd(Vc2RateFilter& f, const float* in, float* out, int channels) {
    using namespace xtf;
    for (int n = 0; n < kQuarter; ++n) {
        for (int x = 0; x < channels; ++x) {
            float& o = out[channels * 2 * n + x];
            o = add(o, interpolated(f.c, f.s + x, mul(f.c[0], in[channels * n + x]), channels));
        }
        for (int x = 0; x < channels; ++x) {
            float& o = out[channels * (2 * n + 1) + x];
            o = add(o, interpolated(f.c, f.s + x, 0.0f, channels));
        }
    }
}

void vc2InterpolateFrames(Vc2RateFilter& f, const float* in, const float* full, float* frames, int channels) {
    using namespace xtf;
    for (int n = 0; n < kFull; ++n)
        for (int x = 0; x < channels; ++x) {
            const float y = interpolated(f.c, f.s + x, n % 2 == 0 ? mul(f.c[0], in[channels * (n / 2) + x]) : 0.0f,
                                         channels);
            frames[2 * n + x] = add(y, f.s[kDelay * channels + x]);
            f.s[kDelay * channels + x] = full[channels * n + x];
        }
}
