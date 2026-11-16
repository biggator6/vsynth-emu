// The VOCODER OSC's carrier: see Vc2Carrier.h (v221).  Each routine's arithmetic in its own order, in XtFloat; every
// address is rom.4's.
#include "Vc2Carrier.h"
#include "Vc2Math.h"
#include "XtFloat.h"

namespace {
using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

// A two-state stage's sample (the saw's two, the noise's high-pass): s1 first, then s2 on what is left, which it returns
float stageStep(Vc2CarrierStage& st, float x, float g) {
    st.s1 = add(mul(st.s2, g), st.s1);
    const float left = sub(sub(x, st.s1), st.s2);
    st.s2 = add(mul(left, g), st.s2);
    return left;
}

// The noise generator's step, after the saw's each sample: two 32-bit words
void noiseStep(Vc2CarrierWords& w) {
    const uint32_t a = (uint32_t)w.noiseA, b = (uint32_t)w.noiseB;
    const uint32_t nextA = a * (uint32_t)-250 + ((b << 7) - b) + 30u;
    w.noiseA = (int32_t)nextA;
    w.noiseB = (int32_t)(((b << 6) - b) * 4u + nextA);
}
} // namespace

float vc2CarrierPhaseStart(int32_t clock) {
    const Vc2CarrierTables& t = vc2CarrierTables();
    const int16_t now = (int16_t)clock;                 // the clock's word as a short
    const int32_t magnitude = now > 0 ? now : -now;     // cmp/pl, then neg: −32768 stays itself as a short
    const float x = div(xtf::flt((int16_t)magnitude), t.clockScale);
    const float r = mul(mul(sub(1.0f, x), x), t.logistic);   // a logistic step from it
    return add(mul(r, 2.0f), -1.0f);
}

void Vc2Carrier::init(const Vc2CarrierParams& p, int32_t clock) {
    const Vc2CarrierTables& t = vc2CarrierTables();
    w.noiseA = w.noiseB = 0;
    w.noiseLow = 0.0f;
    w.sawA = w.noiseHigh = w.sawB = Vc2CarrierStage {};
    settings(&p);
    w.gain = w.gainTarget = p.gain;
    w.step = w.stepTarget = p.pitch;
    w.phase = vc2CarrierPhaseStart(clock);
    w.sawCoef = p.sawCutoff;
    w.noiseLowCoef = p.noiseLowpass;
    w.noiseHighCoef = p.noiseHighpass;
    w.growlPhase = 0.0f;
    w.growlStep = mul(t.pitchFactor, t.growlStart);   // so the first growl cycle starts at the second sample
    w.growl = 0.0f;
}

void Vc2Carrier::settings(const Vc2CarrierParams* p) {
    if (!p) return;
    w.sawCutoff = p->sawCutoff;
    w.sawGain = p->sawLevel;
    w.noiseLowpass = p->noiseLowpass;
    w.noiseHighpass = p->noiseHighpass;
    w.noiseGain = p->noiseLevel;
    w.threshold = p->threshold;
    w.sensitivity = p->sensitivity;
}

void Vc2Carrier::update(const Vc2CarrierParams* p, float peak) {
    const Vc2CarrierTables& t = vc2CarrierTables();
    if (p) {   // the saw's gain is written only below a balance of 1: from 1 up it keeps what it had
        w.stepTarget = p->pitch;
        w.gainTarget = p->gain;
        w.growl = p->growl;
        if (1.0f > p->balance) w.sawGain = vc2Clamp01(mul(p->sawLevel, p->balance));
        w.noiseGain = vc2Clamp01(mul(sub(2.0f, p->balance), p->noiseLevel));
    }
    const float pitch = w.stepTarget;
    float low = w.noiseLowpass;
    if (1.0f > pitch) low = mul(low, add(mul(pitch, t.lowSlope), t.lowBase));
    float held = pitch;   // the high-pass's pitch, held to [minPitch, 2]
    if (held > 2.0f) {
        held = 2.0f;
    } else if (t.minPitch > held) {
        held = t.minPitch;
    }
    const float square = add(mul(mul(held, held), t.highA), w.noiseHighpass);
    const float high = vc2Clamp01(add(add(mul(held, t.highB), square), t.highC));
    float lowScale = 0.0f, sawScale = 0.0f;
    if (t.floor > w.threshold) {   // no Dynamics: the sensitivity alone lowers both
        lowScale = sub(1.0f, mul(w.sensitivity, t.dynLow));
        sawScale = sub(1.0f, mul(w.sensitivity, t.dynSaw));
    } else {   // the input's peak against twice the threshold moves both
        float x = div(peak, mul(w.threshold, 2.0f));
        if (x > 1.0f) x = 1.0f;
        const float d = mul(add(mul(sub(1.0f, w.sensitivity), t.dynHalf), mul(x, w.sensitivity)), 2.0f);
        lowScale = add(mul(sub(1.0f, d), t.dynLow), 1.0f);
        sawScale = add(mul(d, t.dynSaw), t.dynSawBase);
    }
    w.noiseLowCoef = mul(low, lowScale);
    w.noiseHighCoef = high;
    w.sawCoef = mul(w.sawCutoff, sawScale);
    if (w.noiseLowCoef > 1.0f) w.noiseLowCoef = 1.0f;
    if (w.sawCoef > 1.0f) w.sawCoef = 1.0f;
}

float Vc2Carrier::growlSaw(float delta, float overPi) {
    const Vc2CarrierTables& t = vc2CarrierTables();
    w.growlPhase = add(w.growlPhase, w.growlStep);
    if (!(1.0f > w.growlPhase)) {   // a new cycle: its step the saw's over 2 or more, one more by the noise
        w.growlPhase = add(w.growlPhase, t.growl.minusTwo);
        int divider = xtf::ftrc(div(mul(w.step, t.k440), t.k50));
        if (divider < 2) divider = 2;
        if (w.noiseA > 0) ++divider;
        w.growlStep = div(delta, xtf::flt(divider));
    }
    const float level = mul(add(mul(vc2Cosine(t.math, w.growlPhase), w.growl), 1.0f), w.phase);
    float shifted = add(mul(vc2Sine(t.math, w.growlPhase), overPi), w.phase);
    if (!(1.0f > shifted)) {
        shifted = add(shifted, t.growl.minusTwo);
    } else if (-1.0f > shifted) {
        shifted = add(shifted, 2.0f);
    }
    return add(shifted, level);
}

void Vc2Carrier::render(float* frames) {
    const Vc2CarrierTables& t = vc2CarrierTables();
    const bool growls = w.growl > 0.0f;   // tested once, at the first sample
    const Vc2CarrierPool& pool = growls ? t.growl : t.plain;
    const float noiseScale = mul(w.noiseGain, t.noiseScale);
    const float overPi = mul(w.growl, t.overPi);
    for (int i = 0; i < kFrames; ++i) {
        const Vc2CarrierPool& glide = i == 0 ? t.plain : pool;   // the first sample's glide is before the branch
        w.step = add(mul(sub(w.stepTarget, w.step), glide.half), w.step);
        const float delta = mul(w.step, t.pitchFactor);
        w.phase = add(w.phase, delta);
        if (!(1.0f > w.phase)) w.phase = add(w.phase, glide.minusTwo);
        const float x = growls ? mul(growlSaw(delta, overPi), pool.half) : w.phase;
        stageStep(w.sawA, x, w.sawCoef);
        stageStep(w.sawB, w.sawA.s1, w.sawCoef);
        noiseStep(w);
        w.noiseLow = add(mul(sub(mul(xtf::flt(w.noiseA), noiseScale), w.noiseLow), w.noiseLowCoef), w.noiseLow);
        const float noise = stageStep(w.noiseHigh, w.noiseLow, w.noiseHighCoef);
        w.gain = add(mul(sub(w.gainTarget, w.gain), pool.glide), w.gain);
        const float y = mul(mul(add(mul(w.sawB.s1, w.sawGain), noise), w.gain), pool.toBus);
        frames[2 * i] = frames[2 * i + 1] = y;
    }
}
