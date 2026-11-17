// The sub oscillator's shape (v246): rom.4's 0x893c4020, which its note-on (Vc2OscAnalogSub.cpp) calls first; see
// Vc2OscAnalog.h.  rom.2's subShape (XtAnalog): the second wave's pitch ratio 2^Fat into Fat's target, then by the
// code the gain's and level's targets, both waves' Pulse Width targets held against their Hz, the phases' steps and
// a case's end (SQUARE's and JUNO's steepness, RAMP's segments).  The VC-2 narrows the Pulse Width by a line per
// waveform, as its note-on does.  Every address is rom.4's; every offset the voice's.  HARDWARE_FIRMWARE.md, "The
// VC-2's sub oscillator's shape".  Audio thread.
#include "Vc2OscAnalog.h"
#include "XtFloat.h"
#include <array>
#include <cmath>

using xtf::add;
using xtf::div;
using xtf::mul;
using namespace Vc2OscAnalog;

namespace {
constexpr float kTwo = 2.0f;   // [FW] FLDI1 added to itself (fr13)

// [FW] a wave's x from the Pulse Width's sum: its line (FMAC, product then sum), SAW's folded (FABS), SINE's the sum
float lined(float w, const Vc2OscShapeWave& t) { return add(mul(w, t.line.span), t.line.lo); }
float folded(float w, const Vc2OscShapeWave& t) { return std::fabs(lined(w, t)); }
float unlined(float w, const Vc2OscShapeWave&) { return w; }

// [FW] 0x893c440c…, 0x893c44cc… (SQUARE, JUNO): each wave's steepness the rate over its doubled Hz, × 0.008
void steepEnds(Vc2OscVoice& v, float e1, float e2, const Vc2OscShapeWave& t) {
    v.setF32(kSquareSteep, mul(div(t.fs, e1), t.end));
    v.setF32(kSquareSteep2, mul(div(t.fs, e2), t.end));
}

// [FW] 0x893c45fe… (RAMP): each wave's last segment, its doubled Hz over the rate, × 1/16
void rampEnds(Vc2OscVoice& v, float e1, float e2, const Vc2OscShapeWave& t) {
    v.setF32(kRampEnd, mul(div(e1, t.fs), t.end));
    v.setF32(kRampEnd2, mul(div(e2, t.fs), t.end));
}

// [FW] each wave's way, codes 2…7 (0x893c4212, 0x893c4390, 0x893c42a0, 0x893c4550, 0x893c4320, 0x893c4450): its x;
// whether the holds' Hz are doubled; its end
struct Vc2OscShapeWay {
    float (*x)(float, const Vc2OscShapeWave&) = lined;
    bool doubled = false;
    void (*end)(Vc2OscVoice&, float, float, const Vc2OscShapeWave&) = nullptr;
};
constexpr std::array<Vc2OscShapeWay, 6> kWays { {
    { .x = folded },                              // 2 SAW
    { .doubled = true, .end = steepEnds },        // 3 SQUARE
    {},                                           // 4 TRIANGLE
    { .doubled = true, .end = rampEnds },         // 5 RAMP
    { .x = unlined },                             // 6 SINE
    { .doubled = true, .end = steepEnds },        // 7 JUNO
} };
constexpr uint32_t kFirstWave = 2;   // [FW] SAW's code, the first of kWays

// [FW] a wave's Hz the ratio × its own, the second's that × its pitch, each doubled where the way says; each Pulse
// Width target x held within the Hz over the rate × the holds' scale (0x893c4a20's way, on the row's ½), × 0.002; the
// steps; then the way's end
void wave(Vc2OscVoice& v, const Vc2OscShapeIn& in, const Vc2OscShapeWay& way, const Vc2OscShapeWave& t) {
    const float x = way.x(in.w, t);
    const float f = mul(in.r, t.hz);
    const float g = mul(in.ratio2, f);
    const float e1 = way.doubled ? mul(f, kTwo) : f;
    const float e2 = way.doubled ? mul(g, kTwo) : g;
    v.setF32(kPwTarget, mul(withinBy(x, mul(div(e1, t.fs), t.scale), t.half), t.floor));
    v.setF32(kSwTarget, mul(withinBy(x, mul(div(e2, t.fs), t.scale), t.half), t.floor));
    shapeSteps(v, in, t.step);
    if (way.end) way.end(v, e1, e2, t);
}

void codeWave(Vc2OscVoice& v, const Vc2ToneShared&, const Vc2OscShapeIn& in, uint32_t code) {
    wave(v, in, kWays[code - kFirstWave], vc2OscTables().analog.shape.waves[code - kFirstWave]);
}
void laSquareWay(Vc2OscVoice& v, const Vc2ToneShared& s, const Vc2OscShapeIn& in, uint32_t) {
    shapeLaSquare(v, s, in);
}
void laSawWay(Vc2OscVoice& v, const Vc2ToneShared& s, const Vc2OscShapeIn& in, uint32_t) { shapeLaSaw(v, s, in); }

// [FW] NOISE's hold on the head's ½ and 30 (0x893c4140); HQ's none
void noiseHolds(Vc2OscVoice& v, float w, const Vc2OscShapeHead& t) { holdNoise(v, w, t.noiseHalf, t.noiseSpan); }
void noHold(Vc2OscVoice&, float, const Vc2OscShapeHead&) {}

// [FW] each code's way, 0…10: the first switch's (0x893c40c0: HQ and NOISE, the gain as it is and the Pulse Width's
// sum, then NOISE's hold), or the second's (0x893c41e0: the waves and LA's, after the gain's and level's targets);
// past 10 the second's targets alone
struct Vc2OscShapeCode {
    void (*first)(Vc2OscVoice&, float, const Vc2OscShapeHead&) = nullptr;
    void (*second)(Vc2OscVoice&, const Vc2ToneShared&, const Vc2OscShapeIn&, uint32_t) = nullptr;
};
constexpr std::array<Vc2OscShapeCode, 11> kCodes { {
    { .first = noHold },         // 0 HQ-SAW
    { .first = noHold },         // 1 HQ-SQUARE
    { .second = codeWave },      // 2 SAW
    { .second = codeWave },      // 3 SQUARE
    { .second = codeWave },      // 4 TRIANGLE
    { .second = codeWave },      // 5 RAMP
    { .second = codeWave },      // 6 SINE
    { .second = codeWave },      // 7 JUNO
    { .first = noiseHolds },     // 8 NOISE
    { .second = laSquareWay },   // 9 LA-SQUARE
    { .second = laSawWay },      // 10 LA-SAW
} };
constexpr Vc2OscShapeCode kPast {};
} // namespace

// [FW] 0x893c4212…: the phases' steps, the ratio × the case's step and the ratio × the second wave's pitch × it
void Vc2OscAnalog::shapeSteps(Vc2OscVoice& v, const Vc2OscShapeIn& in, float step) {
    v.setF32(kLaStep, mul(in.r, step));
    v.setF32(kSubStep, mul(mul(in.r, in.ratio2), step));
}

// [FW] 0x893c4020: Fat's target 2^(its sum, unheld) by powf; the Pulse Width's sum; then the code's way, the first
// switch's or (the ratio read first) the gain × 2^26 and the sub level × 0.002 into their targets, then the second's
// on the ratio × 2
void Vc2OscAnalog::subShape(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s) {
    const Vc2OscShapeHead& t = vc2OscTables().analog.shape.head;
    const float ratio2 = s.pow(kTwo, rawSum(v, p, kFat, kFatMod));   // [FW]
    v.setF32(kFatTarget, ratio2);
    const float w = sum(v, p, kPw, kPwMod);
    const uint32_t code = v.u32(kCodeCopy);
    const Vc2OscShapeCode& row = code < kCodes.size() ? kCodes[code] : kPast;
    const Vc2OscTargets targets(v, p, s);
    if (row.first) {
        v.setF32(kGainTarget, targets.gain());
        v.setF32(kPwTarget, w);
        row.first(v, w, t);
        return;
    }
    const float ratio = v.f32(kRatio);
    v.setF32(kGainTarget, mul(targets.gain(), t.gainScale));
    v.setF32(kLevelTarget, mul(v.f32(kSubLevel), t.levelStep));
    if (row.second) row.second(v, s, { mul(ratio, kTwo), ratio2, w }, code);
}
