// The VC-2's OSC vibrato: see Vc2OscVibrato.h (v267).  Each routine's arithmetic in rom.4's order, FMAC as a product then
// a sum, in XtFloat; every address is rom.4's.
#include "Vc2OscVibrato.h"
#include "Vc2OscTables.h"
#include "XtFloat.h"
#include <algorithm>

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;

namespace {
constexpr int32_t kScaleOne = 100;                   // [FW] 0x893b2a16: the block's +0x24 at a scale of 1
constexpr int kCountTime = 10;                       // [FW] 0x893b2c1c…: the time goes on by 10 a count
constexpr uint32_t kPhaseMask = 0xffff;              // [FW] 0x893b2cec: extu.w
constexpr int kWholeShift = 7;                       // [FW] 0x893b2ce4: the phase's top 9 bits pick the sample
constexpr uint32_t kFraction = 511;                  // [FW] 0x893b2cf4…: its low 9 bits lie between two samples

// [FW] 0x893b2a76…, 0x893b2bb4…: the step's target within 1 and the most
float heldStep(float x, float most) {
    const float least = 1.0f > x ? 1.0f : x;
    return least > most ? most : least;
}

// The scale by the block's +0x24: a line of each slope either side of 100
float scaleOf(int32_t by, const Vc2OscVibratoPool& t) {
    if (by >= kScaleOne) return add(mul(xtf::flt((int32_t)((uint32_t)by - kScaleOne)), t.scaleUp), 1.0f);
    return sub(1.0f, mul(xtf::flt((int32_t)(kScaleOne - (uint32_t)by)), t.scaleDown));
}

// A wave's word by rom.4's address; the converter's rows hold waves 0 and 1 alone, and past them the last word is read
float waveWord(uint32_t at, const Vc2OscVibratoPool& t) {
    const uint32_t index = (at - kVc2VibratoWavesAt) / 4;
    return t.waves[std::min<size_t>(index, t.waves.size() - 1)];
}
} // namespace

void Vc2OscVibrato::build() {
    w.pitch = 0.0f;
    w.level = 0.0f;
    w.active = 0;
}

void Vc2OscVibrato::noteOn(const Vc2VibratoBlock& b) {
    const Vc2OscVibratoPool& t = vc2OscTables().vibrato;
    w.wave = b.wave;
    w.fadeTarget = 1.0f;
    w.step = 0.0f;
    w.pitch = 0.0f;
    w.level = 0.0f;
    w.falling = 0;
    w.waveAt = kVc2VibratoWavesAt + (uint32_t)b.wave * kVc2VibratoWaveBytes;
    w.scale = scaleOf(b.scaleBy, t);
    w.glide = mul(mul(w.scale, w.scale), t.glide);
    w.stepTarget = heldStep(mul(mul(b.rate, t.stepPerRate), w.scale), t.stepMost);
    w.pitchBias = b.pitchBias;
    w.levelBias = b.levelBias;
    w.rate = b.rate;
    w.time = div(-b.delay, w.scale);
    w.riseHold = add(b.rise, b.hold);
    w.stepRatio = b.stepRatio;
    if (b.rise > 0.0f) {
        w.fade = 0.0f;
        w.fadeRate = div(t.riseTen, b.rise);
    } else {
        w.fade = 1.0f;
        w.step = w.stepTarget;
        const bool holds = w.riseHold > 0.0f;
        w.fadeRate = holds ? 1.0f : 0.0f;
        if (!holds) w.fadeTarget = 0.0f;
    }
    if (b.fall == 0.0f) w.fallRate = 0.0f;
    else w.fallRate = b.fall > 0.0f ? div(1.0f, b.fall) : t.fallShort;
    w.active = 1;
}

void Vc2OscVibrato::settings(const Vc2VibratoBlock& s) {
    const Vc2OscVibratoPool& t = vc2OscTables().vibrato;
    w.pitchBias = s.pitchBias;
    w.levelBias = s.levelBias;
    w.rate = s.rate;
    float target = mul(mul(s.rate, t.stepPerRate), w.scale);
    if (w.falling != 0) target = sub(mul(w.stepRatio, target), w.step);
    w.stepTarget = heldStep(target, t.stepMost);
}

void Vc2OscVibrato::update(int32_t n) {
    if (w.active == 0) return;
    const Vc2OscVibratoPool& t = vc2OscTables().vibrato;
    w.time = add(w.time, xtf::flt(n * kCountTime));
    if (0.0f > w.time) return;
    if (mul(w.fadeTarget, w.time) > w.riseHold) {   // the hold is over: the fall starts, once
        w.fadeRate = mul(w.fallRate, t.fallTen);
        w.stepTarget = sub(mul(w.stepTarget, w.stepRatio), w.step);
        w.time = sub(w.time, w.riseHold);
        w.fadeTarget = 0.0f;
        w.falling = 1;
    }
    if (w.falling != 0) {
        const float x = mul(w.time, w.fallRate);
        w.phase = add(add(mul(w.stepTarget, x > 1.0f ? 1.0f : x), w.step), w.phase);
    } else {
        w.step = add(mul(sub(w.stepTarget, w.step), w.glide), w.step);
        w.phase = add(w.phase, w.step);
    }
    if (w.phase > t.phaseTop) w.phase = add(w.phase, t.phaseBack);
    const uint32_t at = (uint32_t)xtf::ftrc(w.phase) & kPhaseMask;
    const uint32_t sample = w.waveAt + (at >> kWholeShift) * 4;
    const float first = waveWord(sample, t);
    const float between = mul(sub(waveWord(sample + 4, t), first), xtf::flt((int)(at & kFraction)));
    const float wave = add(mul(between, t.perStep), first);
    w.fade = add(mul(mul(sub(w.fadeTarget, w.fade), w.fadeRate), xtf::flt(n)), w.fade);
    w.pitch = mul(add(w.pitchBias, wave), w.fade);
    w.level = mul(add(w.levelBias, wave), w.fade);
}
