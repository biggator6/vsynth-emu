// LA-SQUARE's and LA-SAW's way of the analog note-on (v242) and of its sub oscillator's shape (v246, shapeLaSquare and
// shapeLaSaw, sharing the corner's laws): see Vc2OscAnalog.h.  rom.2's laws (XtAnalog's laShape and
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

// [FW] the Tone Keyfollow's factor, by FTRC of the block's value, from a routine's table: past the table rom.4 reads
// what follows it, which the converter (0…14) never asks; the port reads 0 there
float toneKfOf(const Vc2OscVoice& v, const std::array<float, 15>& table) {
    const int i = xtf::ftrc(v.f32(kToneKf));
    return i >= 0 && i < (int)table.size() ? table[(size_t)i] : 0.0f;
}

// [FW] the Tone's corner, on a routine's pool words: powf(2, Tone / 127 × 7) × 156
float toneCorner(const Vc2OscVoice& v, const Vc2ToneShared& s, float scale, float octaves, float corner) {
    return mul(s.pow(kTwo, mul(div(v.f32(kTone), scale), octaves)), corner);
}

// [FW] a corner keyfollowed: (1 + the Tone KF's factor × (the ratio × its keyfollow ratio − 1)) × the Tone's corner
float keyed(float tk, float ratio, float kfRatio, float base) {
    return mul(add(mul(tk, add(mul(ratio, kfRatio), -kOne)), kOne), base);
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
    const float base = toneCorner(v, s, t.toneScale, t.toneOctaves, t.corner);
    const float c = keyed(toneKfOf(v, t.toneKf), v.f32(kRatio), t.kfRatio, base);
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

// A sub wave's hold (what its Pulse Width is held within) and its steepness
struct Vc2OscLaWave {
    float m, steep;
};

// [FW] 0x893c46a8…, 0x893c48c4… (each wave's): the corner at most 20 kHz; the wave's Hz when it is above it (m ½),
// else m the hold's Hz over the corner; the steepness the corner over its Hz, × 2
Vc2OscLaWave laWave(float c, float hz, float holdHz, float steepHz, const Vc2OscShapeLa& t) {
    if (c > t.most) c = t.most;
    float m = t.half;
    if (hz > c) c = hz;
    else m = div(holdHz, c);
    return { m, mul(div(c, steepHz), kTwo) };
}

// [FW] 0x893c4840 (LA-SQUARE), 0x893c4620 (LA-SAW, on the ratio × ½): the steps; the first wave's corner from the
// Tone keyfollowed by the ratio, its Hz the ratio × 440, its hold's the ratio × 220; the second's keyfollowed by the
// ratio × its pitch, its Hz that × the wave's; the Pulse Width held within each (on the case's ½) and the steepness,
// × 0.002
void shapeLa(Vc2OscVoice& v, const Vc2ToneShared& s, const Vc2OscShapeLa& t, const Vc2OscShapeIn& in) {
    const std::array<float, 15>& table = vc2OscTables().analog.shape.toneKf;
    shapeSteps(v, in, t.step);
    const float hz = mul(in.r, t.hz440);
    const float c1 = keyed(toneKfOf(v, table), in.r, t.kf1, toneCorner(v, s, t.toneScale, t.toneOctaves, t.corner));
    const Vc2OscLaWave one = laWave(c1, hz, mul(in.r, t.hz220), hz, t);
    const float c2 = keyed(toneKfOf(v, table), mul(in.r, in.ratio2), t.kf2,
                           toneCorner(v, s, t.toneScale, t.toneOctaves, t.corner));
    const float pitched = mul(in.ratio2, hz);
    const Vc2OscLaWave two = laWave(c2, hz, pitched, pitched, t);
    v.setF32(kPwTarget, mul(withinBy(in.w, one.m, t.half), t.floor));
    v.setF32(kSwTarget, mul(withinBy(in.w, two.m, t.half), t.floor));
    v.setF32(kSteepTarget, mul(one.steep, t.floor));
    v.setF32(kSteep2Target, mul(two.steep, t.floor));
}
} // namespace

void Vc2OscAnalog::shapeLaSquare(Vc2OscVoice& v, const Vc2ToneShared& s, const Vc2OscShapeIn& in) {
    shapeLa(v, s, vc2OscTables().analog.shape.la.front(), in);
}

void Vc2OscAnalog::shapeLaSaw(Vc2OscVoice& v, const Vc2ToneShared& s, const Vc2OscShapeIn& in) {
    const Vc2OscShapeLa& t = vc2OscTables().analog.shape.la.back();
    shapeLa(v, s, t, { mul(in.r, t.half), in.ratio2, in.w });
}

float Vc2OscAnalog::withinBy(float x, float m, float half) {
    const float top = sub(kOne, m);
    if (half > x) return m > x ? m : x;
    return x > top ? top : x;
}

float Vc2OscAnalog::within(float x, float m, const Vc2OscAnalogPool& t) { return withinBy(x, m, t.withinHalf); }

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
