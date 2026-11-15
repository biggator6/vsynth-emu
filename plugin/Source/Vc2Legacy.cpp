// The VC-2's LEGACY object and its mono synthesis: see Vc2Legacy.h (v211).  Every address is rom.4's; the check is
// xt_vc2legacy's (Vc2LegacyCheck.cpp).  The arithmetic is the SH-4's (XtFloat): an FMAC is the product rounded, then the
// sum.
#include "Vc2Legacy.h"
#include "Vc2LegacyBand.h"
#include "XtFloat.h"
#include <algorithm>
#include <bit>

namespace {
constexpr float k(uint32_t bits) { return std::bit_cast<float>(bits); }
constexpr int kBands = Vc2LegacyTables::kCarrierBands, kBlock = Vc2LegacyAnalysis::kBlock;
// [FW 0x893f2590, 0x893f2594] the constructor's glide (the reset's too) and p
constexpr float kGlideStart = 0.5f, kFormantStart = k(0x3f0001aau);
// [FW 0x893f28c4…0x893f28ec] the mono synthesis's
constexpr float kEnvelopeShare = 0.5f;
constexpr float kLevelKeep = k(0x3f7f9d01u), kLevelTake = k(0x3a45fe74u);   // 0.998489, 0.000755287: the level's
constexpr float kBandsGain = 12.0f, kUnvoicedGain = 16.0f;
constexpr float kTiltOffset = k(0x3efe3c31u);   // 0.496553: the glide's target is 1/(p² + this)
constexpr float kGlideRate = k(0x3c23d709u);    // 0.01 a sample
// [FW 0x893f28f0…0x893f294c] the EQ.  The tilt: its first difference × 3, and four coefficients, each base + glide × slope
struct Moving {
    float base, slope;
};
constexpr float kTiltIn = 3.0f;
constexpr Moving kTilt[4] = {
    { k(0x3ffebfe8u), k(0xbd26ce07u) },   // on its last state
    { k(0xbf7d7fd2u), k(0x3d252f46u) },   // on the one before
    { k(0x3ef29e64u), k(0xbe671f33u) },   // its output's, on the state
    { k(0xbef29e64u), k(0x3e7ac531u) },   // its output's, on the last state
};
constexpr float kPoleIn = k(0x3fe51a9cu), kPoleBack = k(0x3f7d5ffau);   // 1.78987, 0.989746: the first one-pole
constexpr float kPoleZero = k(0xbf7e692cu);                              // −0.993792: its zero
constexpr float kPole2Back = k(0x3dd418ebu);                             // 0.103563: the second one-pole
// two biquads of one form; the last one's symmetric numerator, its feedback and the output's gain
constexpr Vc2Biquad kBiquads[2] = {
    { k(0x3ea54ea3u), k(0x3dc65e5du), k(0xbf3d17c0u), k(0x3e222b78u) },
    { k(0x3efc3b69u), k(0x3e1756d8u), k(0xbf535fdcu), k(0x3eeea68du) },
};
constexpr float kLastOuter = k(0x42f49fccu), kLastMiddle = k(0xc3749fccu);   // 122.312, −244.624
constexpr float kLastBack1 = k(0xbff08aeeu), kLastBack2 = k(0x3f62d93du);    // −1.87924, 0.886127
constexpr float kOutGain = k(0x3bfc30a2u);                                   // 0.00769623
// [FW 0x893f3050, 0x893f3054] slot 4's
constexpr float kFormantOffset = k(0xbc7f9724u), kFormantScale = k(0x3ffbfe5bu);   // −0.0156, 1.9687

// A biquad as rom.4 orders it on its state {x₋₁, y₋₁, y₋₂}: y = (x₋₁·b1 + x·b0) − y₋₁·a1 − y₋₂·a2
float biquad(float* st, const Vc2Biquad& c, float x) {
    using namespace xtf;
    const float y = sub(sub(add(mul(st[0], c.b1), mul(x, c.b0)), mul(st[1], c.a1)), mul(st[2], c.a2));
    st[2] = st[1];
    st[1] = y;
    st[0] = x;
    return y;
}
} // namespace

void Vc2Legacy::clear() {
    for (auto& band : s.bands) std::fill(std::begin(band), std::end(band), 0.0f);
    std::fill(std::begin(s.carrier), std::end(s.carrier), 0.0f);
    std::fill(std::begin(s.eqLow), std::end(s.eqLow), 0.0f);
    std::fill(std::begin(s.eqHigh), std::end(s.eqHigh), 0.0f);
    s.glide = kGlideStart;
}

Vc2Legacy::Vc2Legacy() {
    analysis.reset();
    clear();
    s.formant = kFormantStart;
}

void Vc2Legacy::reset() {
    analysis.reset();
    clear();
}

void Vc2Legacy::formant(float ratio) { s.formant = xtf::div(xtf::add(ratio, kFormantOffset), kFormantScale); }

void Vc2Legacy::settings(float ratio, float dump) {
    formant(ratio);
    analysis.dumpTime(dump);
}

float Vc2Legacy::band(int b, float u, int i) {
    using namespace xtf;
    const Vc2LegacyTables& t = vc2LegacyTables();
    const float* c = &t.carrier[b * Vc2LegacyTables::kCoefficients];
    float* st = s.bands[b];
    const float y = vc2FirstResonator(c, st, u);
    const float z = vc2SecondResonator(c, st, mul(mul(analysis.row(b, i), y), kEnvelopeShare));
    return mul(t.gains[b], z);
}

float Vc2Legacy::eq(float x) {
    using namespace xtf;
    s.glide = add(mul(sub(div(1.0f, add(mul(s.formant, s.formant), kTiltOffset)), s.glide), kGlideRate), s.glide);
    float c[4];
    for (int n = 0; n < 4; ++n) c[n] = add(mul(s.glide, kTilt[n].slope), kTilt[n].base);
    float* low = s.eqLow;
    const float d = sub(x, low[0]);
    low[0] = x;
    const float state = add(mul(c[1], low[2]), add(mul(c[0], low[1]), mul(d, kTiltIn)));
    const float tilted = add(mul(c[3], low[1]), mul(c[2], state));
    low[2] = low[1];
    low[1] = state;
    const float q = add(mul(low[3], kPoleBack), mul(tilted, kPoleIn));
    const float poled = add(mul(low[3], kPoleZero), q);
    low[3] = q;
    low[4] = add(mul(low[4], kPole2Back), poled);
    const float y = biquad(&s.eqHigh[3], kBiquads[1], biquad(&s.eqHigh[0], kBiquads[0], low[4]));
    float* last = &s.eqHigh[6];   // x₋₁, x₋₂, y₋₁, y₋₂
    const float v = add(mul(last[1], kLastOuter), add(mul(last[0], kLastMiddle), mul(y, kLastOuter)));
    const float out = sub(sub(v, mul(last[2], kLastBack1)), mul(last[3], kLastBack2));
    last[3] = last[2];
    last[2] = out;
    last[1] = last[0];
    last[0] = y;
    return mul(out, kOutGain);
}

void Vc2Legacy::mono(float* frames) {
    using namespace xtf;
    for (int i = 0; i < kBlock; ++i) {
        const float x = frames[2 * i];
        const float u = add(sub(x, mul(s.carrier[0], 2.0f)), s.carrier[1]);
        s.carrier[1] = s.carrier[0];
        s.carrier[0] = x;
        for (int b = 0; b < kBands; ++b) s.rows[b][i] = band(b, u, i);
        const float level = add(mul(0.0f > x ? -x : x, kLevelTake), mul(s.carrier[2], kLevelKeep));
        s.rows[kBands][i] = add(level, s.carrier[2]);
        s.carrier[2] = level;
        frames[2 * i] = 0.0f;
    }
    for (int b = 0; b < kBands; ++b)
        for (int i = 0; i < kBlock; ++i) frames[2 * i] = add(s.rows[b][i], frames[2 * i]);
    for (int i = 0; i < kBlock; ++i) {
        const float unvoiced = mul(s.rows[kBands][i], analysis.row(kBands, i));
        frames[2 * i] = eq(add(mul(unvoiced, kUnvoicedGain), mul(frames[2 * i], kBandsGain)));
    }
}
