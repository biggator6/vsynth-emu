// The sub oscillator's note-on (v244): rom.4's 0x893c3b90, the analog note-on's way with the sub oscillator on, after
// its head (Vc2OscAnalog.cpp).  rom.2's subNoteOn (XtAnalog): the shape (0x893c4020, rom.2's subShape; the port's
// from v246, Vc2OscAnalogShape.cpp) sets the targets, and this the current values from them, the phase by the code
// and the second wave's in step.  Every address is rom.4's; every offset the voice's.
// HARDWARE_FIRMWARE.md, "The VC-2's sub oscillator's note-on".  Audio thread.
#include "Vc2OscAnalog.h"
#include "XtFloat.h"
#include <array>

using xtf::add;
using xtf::div;
using xtf::mul;
using xtf::sub;
using namespace Vc2OscAnalog;

namespace {
constexpr float kOne = 1.0f, kTwo = 2.0f;   // [FW] FLDI1 (fr15), and FLDI1 added to itself

// [FW] 0x893c3ba0…: the gain the level × 2^31; the current values from the targets: Fat's (the second wave's pitch)
// as it is, the Pulse Width's and the second wave's /0.002, the level's /(1/32)
void currents(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s, const Vc2OscAnalogSubPool& t) {
    v.setF32(kGain, mul(Vc2OscTargets(v, p, s).level(), t.fullScale));
    v.setU32(kFatNow, v.u32(kFatTarget));
    v.setF32(kPwNow, div(v.f32(kPwTarget), t.step));
    v.setF32(kSwNow, div(v.f32(kSwTarget), t.step));
    v.setF32(kLevelNow, div(v.f32(kLevelTarget), t.levelStep));
}

// [FW] SQUARE's and JUNO's steepness of each wave: its value beside its target, /0.002
void steep(Vc2OscVoice& v, const Vc2OscAnalogSubPool& t) {
    for (uint32_t at : { kSquareSteep, kSquareSteep2 }) v.setF32(at + 4, div(v.f32(at), t.step));
}

// [FW] the phase's moves by the code: the Pulse Width held within RAMP's last segment, × ½, added (0x893c3ca0: SAW,
// TRIANGLE, SINE); with the steepness and ½ more (0x893c3c42, SQUARE); 0 added (0x893c3cc0, RAMP), with the
// steepness (0x893c3c80, JUNO)
float held(Vc2OscVoice& v, float p, const Vc2OscAnalogPool& a) {
    return add(mul(within(v.f32(kPwNow), v.f32(kRampEnd), a), a.sub.half), p);
}
float squareMove(Vc2OscVoice& v, float p, const Vc2OscAnalogPool& a) {
    steep(v, a.sub);
    return add(held(v, p, a), a.sub.half);
}
float plusZero(Vc2OscVoice&, float p, const Vc2OscAnalogPool&) { return add(p, 0.0f); }   // [FW] FLDI0, FADD
float junoMove(Vc2OscVoice& v, float p, const Vc2OscAnalogPool& a) {
    steep(v, a.sub);
    return plusZero(v, p, a);
}

// [FW] 0x893c3e20: the phase and the second wave's, Fat (its pitch) × it; both 0 outside 0…1
void phaseEnd(Vc2OscVoice& v, Vc2OscParts&, const Vc2ToneShared&, float p) {
    if (0.0f > p || p > kOne) {
        v.setF32(kPhase, 0.0f);
        v.setF32(kSubPhase, 0.0f);
        return;
    }
    v.setF32(kPhase, p);
    v.setF32(kSubPhase, mul(v.f32(kFatTarget), p));
}

// [FW] 0x893c3ddc (NOISE): the gain the level as it is, the Pulse Width's value its target, the phase alone, 0
// outside 0…1; 0x893c3dd0 (HQ) after HQ's clear
void plainEnd(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s, float ph) {
    v.setF32(kGain, Vc2OscTargets(v, p, s).level());
    v.setU32(kPwNow, v.u32(kPwTarget));
    v.setF32(kPhase, 0.0f > ph || ph > kOne ? 0.0f : ph);
}
void hqEnd(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s, float ph) {
    hqClear(v, ph, vc2OscTables().analog);
    plainEnd(v, p, s, ph);
}

// [FW] 0x893c3cd0 (LA): the steepness' values /0.002; the phase (the Pulse Width held against +0x298) × ½ + p + ½, 0
// outside 0…1, the second wave's Fat × it; the second and third phases (½ held against +0x298, +0x2a0) × ½ less each
// phase, × 2 + 2, less 2 above 1
void laEnd(Vc2OscVoice& v, Vc2OscParts&, const Vc2ToneShared&, float p) {
    const Vc2OscAnalogPool& a = vc2OscTables().analog;
    const Vc2OscAnalogSubPool& t = a.sub;
    v.setF32(kSteepNow, div(v.f32(kSteepTarget), t.step));
    v.setF32(kSteep2Now, div(v.f32(kSteep2Target), t.step));
    float x = add(add(mul(within(v.f32(kPwNow), v.f32(kUnwritten), a), t.half), p), t.half);
    if (0.0f > x || x > kOne) x = 0.0f;
    v.setF32(kPhase, x);
    v.setF32(kSubPhase, mul(v.f32(kFatTarget), x));
    const float w2 = mul(within(t.half, v.f32(kUnwritten), a), t.half);
    v.setF32(kPhase2, add(mul(sub(w2, x), kTwo), kTwo));
    const float w3 = mul(within(t.half, v.f32(kUnwritten2), a), t.half);
    v.setF32(kPhase3, add(mul(sub(w3, mul(v.f32(kFatTarget), x)), kTwo), kTwo));
    for (uint32_t at : { kPhase2, kPhase3 }) {
        if (v.f32(at) > kOne) v.setF32(at, add(v.f32(at), t.wrap));
    }
}

// [FW] each code's way, 0…10 (the switch at 0x893c3c18; past 10 the phase as it is): the phase's move, then its end
struct Vc2OscSubCode {
    float (*move)(Vc2OscVoice&, float, const Vc2OscAnalogPool&) = nullptr;
    void (*end)(Vc2OscVoice&, Vc2OscParts&, const Vc2ToneShared&, float) = phaseEnd;
};
constexpr std::array<Vc2OscSubCode, 11> kSubCodes { {
    { .end = hqEnd },              // 0 HQ-SAW
    { .end = hqEnd },              // 1 HQ-SQUARE
    { .move = held },              // 2 SAW
    { .move = squareMove },        // 3 SQUARE
    { .move = held },              // 4 TRIANGLE
    { .move = plusZero },          // 5 RAMP
    { .move = held },              // 6 SINE
    { .move = junoMove },          // 7 JUNO
    { .end = plainEnd },           // 8 NOISE
    { .end = laEnd },              // 9 LA-SQUARE
    { .end = laEnd },              // 10 LA-SAW
} };
constexpr Vc2OscSubCode kPast {};
} // namespace

// [FW] 0x893c3b90: the shape, the current values, the phase × Impact below 1, then the code's way
void Vc2OscAnalog::subNoteOn(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s) {
    const Vc2OscAnalogPool& a = vc2OscTables().analog;
    subShape(v, p, s);
    currents(v, p, s, a.sub);
    float ph = v.f32(kPhase);
    if (kOne > v.f32(kImpactNow)) ph = mul(ph, v.f32(kImpactNow));
    const uint32_t code = v.u32(kCodeCopy);
    const Vc2OscSubCode& row = code < kSubCodes.size() ? kSubCodes[code] : kPast;
    if (row.move) ph = row.move(v, ph, a);
    row.end(v, p, s, ph);
}
