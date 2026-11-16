// The VOCODER OSC's converter's own part and kind 5's update: see Vc2VocoderOsc.h (v222).  Each routine's arithmetic in
// its own order, in XtFloat; every address is rom.4's.
#include "Vc2VocoderOsc.h"
#include "XtFloat.h"
#include <cstring>

namespace {
using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;

// The converter's input as the SH-4 reads it: little-endian, a byte or a halfword, signed or not by its load
constexpr int kOscAt = 2, kKeyAt = 3, kVelocityAt = 4;   // [FW] the note-on's header
uint8_t u8(const uint8_t* in, int at) { return in[at]; }
int s8(const uint8_t* in, int at) { return (int8_t)in[at]; }
int s16(const uint8_t* in, int at) {
    int16_t v = 0;
    std::memcpy(&v, in + at, sizeof v);
    return v;
}

// [FW] the levels at v/127, each the record's byte (the input's +8 on) to its word of P
struct Level {
    int at;
    float Vc2CarrierParams::*field;
};
constexpr Level kLevels[] = {
    { 0x6c, &Vc2CarrierParams::sawCutoff }, { 0x6d, &Vc2CarrierParams::sawLevel },
    { 0x6e, &Vc2CarrierParams::noiseLowpass }, { 0x6f, &Vc2CarrierParams::noiseHighpass },
    { 0x70, &Vc2CarrierParams::noiseLevel }, { 0x7c, &Vc2CarrierParams::threshold },
    { 0x7d, &Vc2CarrierParams::sensitivity },
};
// [FW] the growl's and the balance's bytes: the level, keyfollow (×10, a halfword), LFO and envelope depths, and the
// velocity curve and its sens (a halfword) the envelope depth is scaled by
constexpr int kGrowlAt = 0x72, kGrowlFollowAt = 0x74, kGrowlLfoAt = 0x76, kGrowlEnvAt = 0x77;
constexpr int kGrowlCurveAt = 0x78, kGrowlSensAt = 0x7a;
constexpr int kBalanceAt = 0x7e, kBalanceLfoAt = 0x82, kBalanceEnvAt = 0x83, kBalanceCurveAt = 0x84;
constexpr int kBalanceSensAt = 0x86;
constexpr int kTime = 1, kFormant = 2, kLfoTime = 2, kLfoFormant = 3;   // the groups' sums the converter reads
constexpr int kDepthMax = 63, kKeyCentre = 60;                          // [FW] immediates
constexpr int32_t kBalanceIndex[2] = { 3, 7 };                          // [FW] P+0x50 by OSC

// 0x8939c0b0: a depth −63…+63 as sign·(d/63)²
float depthCurve(int d) {
    const float x = div(flt((int16_t)d), vc2CarrierTables().depthScale);
    return mul(d < 0 ? -x : x, x);
}

// An LFO depth with the Matrix's sum, held to ±63, through the curve
float lfoDepth(int depth, int sum) {
    int d = sum + depth;
    if (d > kDepthMax) {
        d = kDepthMax;
    } else if (d < -kDepthMax) {
        d = -kDepthMax;
    }
    return depthCurve(d);
}

// The velocity factor an envelope depth is scaled by: the shared curve's word, unsigned, ÷ 32768
float velocityFactor(const uint8_t* in, const Vc2ToneShared& shared, int sensAt, int curveAt) {
    const int v = shared.velocity(u8(in, kVelocityAt), s16(in, sensAt), u8(in, curveAt));
    return div(flt((uint16_t)v), vc2CarrierTables().velocityScale);
}

// A value held to [0, top]: the upper bound first, as the converter tests them
float heldBelow(float x, float top) {
    if (x > top) return top;
    return 0.0f > x ? 0.0f : x;
}

// A level with the Matrix's sum: sum ÷ 12000 + v ÷ scale, held to [0, top]
float levelWithSum(int sum, int v, float scale, float top) {
    return heldBelow(add(div(flt(sum), vc2CarrierTables().sumScale), div(flt(v), scale)), top);
}

// A modulated term as kind 5's update forms it: base + env·envDepth + lfo·lfoDepth, held to [0, top], 0 first
float modulated(float base, float env, float envDepth, float lfo, float lfoDepth, float top) {
    float x = add(mul(lfo, lfoDepth), add(mul(env, envDepth), base));
    if (0.0f > x) x = 0.0f;
    return x > top ? top : x;
}
} // namespace

void vc2VocoderOscConvert(const uint8_t* in, const Vc2MatrixGroups& sums, const Vc2ToneShared& shared,
                          Vc2VocoderOscP& p) {
    const Vc2CarrierTables& t = vc2CarrierTables();
    for (const Level& level : kLevels) p.carrier.*level.field = mul(flt(u8(in, level.at)), t.perLevel);
    Vc2VocoderOscTerms& m = p.terms;
    const float growlVelocity = velocityFactor(in, shared, kGrowlSensAt, kGrowlCurveAt);
    const float growl = levelWithSum(sums.osc[kTime], s8(in, kGrowlAt), t.growlScale, 1.0f);
    const int32_t follow = (u8(in, kKeyAt) - kKeyCentre) * s16(in, kGrowlFollowAt);   // Keyfollow ×10 by key
    m.growl = heldBelow(add(div(flt(follow), t.keyfollowScale), growl), 1.0f);
    m.growlZero[0] = m.growlZero[1] = 0;
    m.growlEnv = mul(depthCurve(s8(in, kGrowlEnvAt)), growlVelocity);
    m.growlLfo = lfoDepth(s8(in, kGrowlLfoAt), sums.lfo[kLfoTime]);
    const float balanceVelocity = velocityFactor(in, shared, kBalanceSensAt, kBalanceCurveAt);
    m.balance = levelWithSum(sums.osc[kFormant], s8(in, kBalanceAt), t.balanceScale, 2.0f);
    m.balanceIndex = kBalanceIndex[u8(in, kOscAt) == 0 ? 0 : 1];
    m.balanceEnv = mul(depthCurve(s8(in, kBalanceEnvAt)), balanceVelocity);
    m.balanceZero = 0;
    m.balanceLfo = lfoDepth(s8(in, kBalanceLfoAt), sums.lfo[kLfoFormant]);
}

void vc2Kind5Update(const Vc2Kind5Voice& v, const Vc2ToneShared& shared, Vc2VocoderOscP& p, Vc2Carrier& carrier,
                    float peak) {
    const Vc2CarrierTables& t = vc2CarrierTables();
    const Vc2VocoderOscTerms& m = p.terms;
    p.carrier.growl = modulated(m.growl, v.growlEnv, m.growlEnv, v.growlLfo, m.growlLfo, 1.0f);
    p.carrier.balance = modulated(m.balance, v.balanceEnv, m.balanceEnv, v.balanceLfo, m.balanceLfo, 2.0f);
    const float gain = add(mul(v.gainMod, v.gainModDepth), 1.0f);
    p.carrier.gain = mul(mul(mul(gain, v.gain[0]), v.gain[1]), v.gain[2]);
    float cents = add(mul(v.pitchMod, v.pitchModDepth), add(mul(v.pitchEnv, v.pitchEnvDepth), v.cents));
    cents = add(add(cents, v.ramp), v.centsOffset);
    if (cents > t.centsMax) {
        cents = t.centsMax;
    } else if (t.centsMin > cents) {
        cents = t.centsMin;
    }
    float ratio = mul(mul(shared.exp2(mul(cents, t.perOctave)), v.ratio), v.ratio2);
    if (ratio > t.ratioMax) {
        ratio = t.ratioMax;
    } else if (t.ratioMin > ratio) {
        ratio = t.ratioMin;
    }
    p.carrier.pitch = ratio;
    carrier.update(&p.carrier, peak);
}
