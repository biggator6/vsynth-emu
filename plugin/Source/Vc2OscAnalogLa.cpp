// LA-SQUARE's and LA-SAW's way of the analog note-on (v242): see Vc2OscAnalog.h.  rom.2's laws (XtAnalog's laShape and
// laNoteOn): the corner from the Tone, its keyfollow and the ratio, at most 20 kHz and at least twice the wave's Hz;
// the gains and shapes as the routine's per-sample moves take them; the phases from Fat and Impact.  rom.4 takes the
// corner by powf(2, …) where rom.2 took expf, and holds the phases' terms by 0x893c4a20 against its +0x298, which the
// note-on doesn't write (rom.2's +0x20c, 0 there).
#include "Vc2OscAnalog.h"
#include "XtFloat.h"

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;
using namespace Vc2OscAnalog;

namespace {
constexpr float kOne = 1.0f, kTwo = 2.0f;   // [FW] FLDI1, and FLDI1 added to itself
constexpr uint32_t kLaFirst = 9;            // [FW] LA-SQUARE's code; LA-SAW's 10

// [FW] the Tone Keyfollow's factor, by FTRC of the block's value: past the table rom.4 reads what follows it, which the
// converter (0…14) never asks; the port reads 0 there
float toneKfOf(const Vc2OscVoice& v, const Vc2OscAnalogPool& t) {
    const int i = xtf::ftrc(v.f32(kToneKf));
    return i >= 0 && i < (int)t.toneKf.size() ? t.toneKf[(size_t)i] : 0.0f;
}

// The corner, the wave's Hz, and what the Pulse Width is held within against them
struct Vc2OscLaCorner {
    float corner, hz, m;
};

// [FW] 0x893c24c8 (LA-SQUARE), 0x893c2530 (LA-SAW): the corner 156 × 2^(7·Tone/127) × (1 + the Tone KF's factor ×
// (the ratio × 2^(9/12) − 1)); 0x893c2596: at most 20 kHz (m the Hz over it), at least twice the Hz (m ½), else m
// the Hz over the corner
Vc2OscLaCorner cornerOf(const Vc2OscVoice& v, const Vc2ToneShared& s, const Vc2OscAnalogLa& la) {
    const Vc2OscAnalogPool& t = vc2OscTables().analog;
    const float hz = mul(v.f32(kRatio), la.hz);
    const float base = mul(s.pow(kTwo, mul(div(v.f32(kTone), t.toneScale), t.toneOctaves)), t.corner);
    const float c = mul(add(mul(toneKfOf(v, t), add(mul(v.f32(kRatio), t.kfRatio), -kOne)), kOne), base);
    if (c > t.cornerMost) return { t.cornerMost, hz, div(hz, t.cornerMost) };
    const float two = mul(hz, kTwo);
    if (two > c) return { two, hz, t.laHalf };
    return { c, hz, div(hz, c) };
}

// [FW] 0x893c2682…: the current values the targets (/0.002); the phase ¼, under Impact below 1 that × Impact + (the
// Pulse Width held, ×½ + ½) × Fat, 0 outside 0…1; the second phase ((½ held) × ½ + ½) × Fat less the phase, × 4 + 1,
// less 2 above 1
void phases(Vc2OscVoice& v, const Vc2OscAnalogPool& t) {
    v.setF32(kPhase, t.phaseStart);
    v.setF32(kPhase2, t.half);
    v.setF32(kPwNow, div(v.f32(kPwTarget), t.step));
    v.setF32(kFatNow, div(v.f32(kFatTarget), t.step));
    v.setF32(kSteepNow, div(v.f32(kSteepTarget), t.step));
    float ph = v.f32(kPhase);
    if (kOne > v.f32(kImpactNow)) {
        ph = mul(ph, v.f32(kImpactNow));
        const float a = add(mul(within(v.f32(kPwNow), v.f32(kUnwritten), t), t.half), t.laHalf);
        ph = add(mul(a, v.f32(kFatNow)), ph);
    }
    if (0.0f > ph || ph > kOne) ph = 0.0f;
    v.setF32(kPhase, ph);
    const float b = add(mul(within(t.half, v.f32(kUnwritten), t), t.half), t.laHalf);
    const float z = add(mul(sub(mul(b, v.f32(kFatNow)), ph), t.four), kOne);
    v.setF32(kPhase2, z);
    if (z > kOne) v.setF32(kPhase2, add(z, t.wrap));
}
} // namespace

float Vc2OscAnalog::within(float x, float m, const Vc2OscAnalogPool& t) {
    const float top = sub(kOne, m);
    if (t.withinHalf > x) return m > x ? m : x;
    return x > top ? top : x;
}

// [FW] 0x893c2404: Fat's half, at least its floor, and the Pulse Width's sum; the corner; then the gain's target
// −gain × 2^26, the phase's step the ratio × its row's, the shapes' targets × 0.002 (the Pulse Width held within m), the
// gain −level × 2^31; then the phases
void Vc2OscAnalog::laNoteOn(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s, uint32_t code) {
    const Vc2OscAnalogPool& t = vc2OscTables().analog;
    const Vc2OscAnalogLa& la = t.la[code - kLaFirst];
    float half = sub(t.laHalf, mul(sum(v, p, kFat, kFatMod), t.laHalf));
    if (!(half > t.laFatMin)) half = t.laFatFloor;
    const float x = add(mul(sum(v, p, kPw, kPwMod), t.laPwSpan), t.laPwLo);
    const Vc2OscLaCorner c = cornerOf(v, s, la);
    const Vc2OscTargets targets(v, p, s);
    v.setF32(kGainTarget, mul(-targets.gain(), t.gainScale));
    v.setF32(kLaStep, mul(v.f32(kRatio), la.step));
    v.setF32(kPwTarget, mul(within(x, c.m, t), t.step));
    v.setF32(kFatTarget, mul(half, t.step));
    v.setF32(kSteepTarget, mul(div(c.corner, c.hz), t.step));
    v.setF32(kGain, mul(-targets.level(), t.fullScale));
    phases(v, t);
}
