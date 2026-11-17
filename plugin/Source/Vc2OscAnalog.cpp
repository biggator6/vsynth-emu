// The analog OSC's note-on, its head and the other codes' way (v242): see Vc2OscAnalog.h.  rom.2's laws (XtAnalog's
// noteOn and shape) but for the Pulse Width's target, which the VC-2 makes narrower by waveform (a line lo + x × span
// for SQUARE, TRIANGLE, RAMP and JUNO, SAW folded and squeezed), and the decay, rom.4's powf where rom.2 took expf.
#include "Vc2OscAnalog.h"
#include "XtFloat.h"
#include <algorithm>
#include <array>

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;
using namespace Vc2OscAnalog;

namespace {
constexpr uint32_t kLaFirst = 9, kLaCodes = 2;  // [FW] the switch at 0x893c23ca: codes 9 and 10 take LA's way
constexpr uint32_t kEnvelopeOut = 0x4c, kLfoOut = 0x38;   // [FW] a bound envelope's and LFO's outputs
constexpr float kOne = 1.0f, kTwo = 2.0f;       // [FW] FLDI1 (fr15), and FLDI1 added to itself
// [FW] the targets into the object, in rom.4's order: the ratio (0x893afb40), the gain (0x893aff30), the level
// (0x893aff60)
constexpr std::array<Vc2OscTargets::Word, 3> kTargets { { { kRatio, &Vc2OscTargets::ratio },
                                                         { kGainTarget, &Vc2OscTargets::gain },
                                                         { kGain, &Vc2OscTargets::level } } };

// [FW] 0x893c28f0, 0x893c2ed0: SAW's, folded to the upper half, × 0.2 + 0.4, widened by the Slope × the ratio × 0.01,
// at most 1 less that
float sawShape(const Vc2OscVoice& v, float x, const Vc2OscCodeWords& t) {
    if (t.half > x) x = sub(kOne, x);
    const float s = mul(mul(v.f32(kRatio), v.f32(kSlope)), t.sawSlope);
    const float y = add(add(mul(x, t.sawSpan), s), t.sawLo);
    const float top = sub(kOne, s);
    return y > top ? top : y;
}

// [FW] 0x893c29d0, 0x893c2fc0
float sineShape(const Vc2OscVoice&, float x, const Vc2OscCodeWords& t) { return mul(x, t.half); }

// [FW] 0x893c2964…, 0x893c2f34…: SQUARE's step by the ratio: none below the least, the first below 1, then the rows',
// else the top
float squareStep(float r, const Vc2OscCodeWords& t) {
    if (t.squareLeast > r) return 0.0f;
    if (kOne > r) return t.squareFirst;
    const auto row = std::ranges::find_if(t.squareSteps, [r](const Vc2OscAnalogStep& s) { return s.bound > r; });
    return row != t.squareSteps.end() ? row->step : t.squareTop;
}

// [FW] 0x893c29b4, 0x893c2f84: SQUARE's edge, the Slope × (the ratio × its step) × 220/44100
void squareEdge(Vc2OscVoice& v, float, const Vc2OscCodeWords& t) {
    const float r = v.f32(kRatio);
    v.setF32(kEdge, mul(mul(v.f32(kSlope), mul(r, squareStep(r, t))), t.edgeScale));
}

// [FW] 0x893c2a26…, 0x893c2ff8…: NOISE's hold, on the routine's own words
void noiseHold(Vc2OscVoice& v, float x, const Vc2OscCodeWords& t) { holdNoise(v, x, t.half, t.noiseSpan); }

// [FW] each code's way, 0…8 (the switches at 0x893c2868 and 0x893c2e6a): the Pulse Width's target a line of x, its
// own law or x, then what else both routines write; HQ's clear the note-on's alone
struct Vc2OscAnalogCode {
    Vc2OscAnalogLine Vc2OscCodeWords::*line = nullptr;
    float (*shape)(const Vc2OscVoice&, float, const Vc2OscCodeWords&) = nullptr;
    void (*then)(Vc2OscVoice&, float, const Vc2OscCodeWords&) = nullptr;
    bool clearsHq = false;
};
constexpr std::array<Vc2OscAnalogCode, 9> kCodes { {
    { .clearsHq = true },                                        // 0 HQ-SAW
    { .clearsHq = true },                                        // 1 HQ-SQUARE
    { .shape = sawShape },                                       // 2 SAW
    { .line = &Vc2OscCodeWords::square, .then = squareEdge },    // 3 SQUARE
    { .line = &Vc2OscCodeWords::triangle },                      // 4 TRIANGLE
    { .line = &Vc2OscCodeWords::ramp },                          // 5 RAMP
    { .shape = sineShape },                                      // 6 SINE
    { .line = &Vc2OscCodeWords::juno },                          // 7 JUNO
    { .then = noiseHold },                                       // 8 NOISE
} };

float pwTargetOf(const Vc2OscVoice& v, float x, const Vc2OscAnalogCode& c, const Vc2OscCodeWords& t) {
    if (c.line) return add(mul(x, (t.*c.line).span), (t.*c.line).lo);
    return c.shape ? c.shape(v, x, t) : x;
}

// [FW] 0x893c2780: Fat's target and value from its sum, at least its floor; the Pulse Width's target by the code's
// way, HQ's clear (past 8 every shape ½); then 0x893c2aa8, its value the target
void otherCodes(Vc2OscVoice& v, Vc2OscParts& p, uint32_t code) {
    const Vc2OscCodeWords& t = vc2OscTables().analog.codes;
    const float half = sub(t.half, mul(sum(v, p, kFat, kFatMod), t.half));
    v.setF32(kFatTarget, half);
    v.setF32(kFatNow, half);
    if (!(v.f32(kFatTarget) > t.fatMin)) {
        v.setF32(kFatTarget, t.fatFloor);
        v.setF32(kFatNow, t.fatFloor);
    }
    const float x = add(mul(sum(v, p, kPw, kPwMod), t.pwSpan), t.pwLo);
    if (code < kCodes.size()) {
        codeTargets(v, x, code, t);
        if (kCodes[code].clearsHq) hqClear(v, x, vc2OscTables().analog);
    } else {
        for (uint32_t at : { kFatTarget, kFatNow, kPwTarget, kPwNow }) v.setF32(at, t.half);
    }
    v.setU32(kPwNow, v.u32(kPwTarget));
}
} // namespace

void Vc2OscAnalog::codeTargets(Vc2OscVoice& v, float x, uint32_t code, const Vc2OscCodeWords& t) {
    if (code >= kCodes.size()) return;
    v.setF32(kPwTarget, pwTargetOf(v, x, kCodes[code], t));
    if (kCodes[code].then) kCodes[code].then(v, x, t);
}

// [FW] 0x893c3570 on the object: HQ's integrator and direction cleared
void Vc2OscAnalog::hqClear(Vc2OscVoice& v, float, const Vc2OscAnalogPool&) {
    v.setF32(kHq, 0.0f);
    v.setU32(kHqFalling, 0);
}

// [FW] 0x893c2a26…, 0x893c4140…: NOISE's hold, 1 + 30·|1 − 2x| samples, as a byte, 1 for 0
void Vc2OscAnalog::holdNoise(Vc2OscVoice& v, float x, float half, float span) {
    const float d = sub(kOne, mul(x, kTwo));
    const int n = half > x ? xtf::ftrc(add(mul(d, span), kOne)) : xtf::ftrc(sub(kOne, mul(d, span)));
    v.setU8(kNoiseHold, (uint8_t)n);
    if (v.u8(kNoiseHold) == 0) v.setU8(kNoiseHold, 1);
}

float Vc2OscAnalog::rawSum(const Vc2OscVoice& v, Vc2OscParts& p, uint32_t base, const Vc2OscTargets::Mod& m) {
    float x = add(mul(p.boundFloat(v.tail.binds[m.envelope], kEnvelopeOut), v.f32(m.envelopeDepth)), v.f32(base));
    x = add(mul(p.boundFloat(v.tail.binds[m.lfo], kLfoOut), v.f32(m.lfoDepth)), x);
    return x;
}

float Vc2OscAnalog::sum(const Vc2OscVoice& v, Vc2OscParts& p, uint32_t base, const Vc2OscTargets::Mod& m) {
    const float x = rawSum(v, p, base, m);
    if (0.0f > x) return 0.0f;
    return x > kOne ? kOne : x;
}

// [FW] 0x893c22e0: the code's copy, the targets, the flags 0, Impact and its decay, the phase scaled by Impact below
// 1 and 0 outside 0…1; then the sub oscillator's note-on and no more, or LA's way, or the other codes'
void Vc2OscVoice::noteOnAnalog(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscAnalogPool& t = vc2OscTables().analog;
    setU32(kCodeCopy, u32(kCode));
    Vc2OscTargets(*this, parts, shared).writeInto(*this, kTargets);
    setU32(kFlags, 0);
    setU32(kImpactNow, u32(kImpact));
    setF32(kDecay, shared.pow(t.impactBase, div(f32(kRatio), t.impactPeriod)));
    if (kOne > f32(kImpactNow)) setF32(kPhase, mul(f32(kPhase), f32(kImpactNow)));
    if (f32(kPhase) > kOne || 0.0f > f32(kPhase)) setF32(kPhase, 0.0f);
    if (xtf::ftrc(f32(kSubOn)) != 0) {
        subNoteOn(*this, parts, shared);
        return;
    }
    const uint32_t code = u32(kCodeCopy);
    if (code - kLaFirst < kLaCodes) laNoteOn(*this, parts, shared, code);
    else otherCodes(*this, parts, code);
}
