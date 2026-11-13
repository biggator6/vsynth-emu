// The VOCODER OSC's growl, as read from the carrier's render: vc2growllaw.h.  Each step is one SH-4 instruction's rounding.
#include "vc2growllaw.h"

#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON

namespace vc2 {

namespace {

// 0x893f3210: a phase in [−1, 1) folded into [−0.5, 0.5], then x (3 − 4 x²): a sine of π x, peak 1 at ±0.5
float sineOf(const GrowlState& k, float x) {
    if (x > k.foldAt) x = 1.0f - x;
    else if (k.foldNeg > x) x = -1.0f - x;
    return (k.three - x * x * k.four) * x;
}

// 0x893f3280: the same cubic on 0.5 − |x|, a cosine of π x
float cosineOf(const GrowlState& k, float x) {
    const float t = k.foldAt - (x < 0 ? -x : x);
    return (k.three - t * t * k.four) * t;
}

int32_t wrapped(int64_t v) {   // the SH-4's 32-bit integer arithmetic
    return (int32_t)(uint32_t)(uint64_t)v;
}

} // namespace

GrowlState growlStateOf(Machine& m, uint32_t carrier) {
    GrowlState s;
    s.target = m.f32(carrier + 0x08);
    s.step = m.f32(carrier + 0x0c);
    s.phase = m.f32(carrier + 0x10);
    s.growl = m.f32(carrier + 0x3c);
    s.growlPhase = m.f32(carrier + 0x40);
    s.growlStep = m.f32(carrier + 0x44);
    s.noiseA = (int32_t)m.cpu.rd32(carrier + 0x64);
    s.noiseB = (int32_t)m.cpu.rd32(carrier + 0x68);
    s.pitchFactor = m.f32(0x899d9964);
    s.half = m.f32(0x893ec230);
    s.minusTwo = m.f32(0x893ec234);
    s.k440 = m.f32(0x893ec4b0);
    s.k50 = m.f32(0x893ec4b4);
    s.foldAt = m.f32(0x893f32ec);
    s.foldNeg = m.f32(0x893f3318);
    s.three = m.f32(0x893f3310);
    s.four = m.f32(0x893f3314);
    const RoundToZero rz;
    s.growlOverPi = s.growl * m.f32(0x893ec4a0);
    return s;
}

float growlStep(GrowlState& s) {
    const RoundToZero rz;
    s.step = fmac(s.target - s.step, s.half, s.step);   // the step glides halfway to its target each sample
    const float delta = s.step * s.pitchFactor;
    s.phase = s.phase + delta;
    if (!(1.0f > s.phase)) s.phase = s.phase + s.minusTwo;
    s.growlPhase = s.growlPhase + s.growlStep;
    if (!(1.0f > s.growlPhase)) {   // a new growl cycle: its step is the saw's over 2 or more, one more by the noise
        s.growlPhase = s.growlPhase + s.minusTwo;
        const float q = s.step * s.k440 / s.k50;
        int divider = q < 2.0f ? 2 : (int)q;
        if (s.noiseA > 0) ++divider;
        s.growlStep = delta / (float)divider;
    }
    const float level = fmac(cosineOf(s, s.growlPhase), s.growl, 1.0f) * s.phase;
    float shifted = fmac(sineOf(s, s.growlPhase), s.growlOverPi, s.phase);
    if (!(1.0f > shifted)) shifted = shifted + s.minusTwo;
    else if (-1.0f > shifted) shifted = shifted + 2.0f;
    const float saw = shifted + level;
    // the noise generator steps after the saw in each sample (0x893ec34c)
    s.noiseA = wrapped((int64_t)s.noiseA * -250 + (int64_t)s.noiseB * 127 + 30);
    s.noiseB = wrapped((int64_t)s.noiseB * 252 + s.noiseA);
    return saw;
}

} // namespace vc2
