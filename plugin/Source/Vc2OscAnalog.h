#pragma once
// The analog OSC's note-on (v242): rom.4's 0x893c22e0, kind 3's, in the analog OSC's code; the kinds' note-on reaches
// it by its row (Vc2OscNoteOn::analog).  Its head and the other codes' way in Vc2OscAnalog.cpp, LA-SQUARE's and
// LA-SAW's in Vc2OscAnalogLa.cpp, the sub oscillator's note-on (0x893c3b90, v244) in Vc2OscAnalogSub.cpp and its
// shape (0x893c4020, v246) in Vc2OscAnalogShape.cpp, the shape's LA way beside the note-on's in Vc2OscAnalogLa.cpp.
// rom.4's words are rom.2's analog voice's (XtAnalog.h) at +0x8c, its block's at +0x30.  Every address is rom.4's;
// every offset the voice's.  HARDWARE_FIRMWARE.md, "The VC-2's analog note-on".  Audio thread.
#include "Vc2OscTables.h"
#include "Vc2OscTargets.h"
#include "Vc2OscVoice.h"
#include <optional>

namespace Vc2OscAnalog {
// [FW] the installs' render routines by code, the sub oscillator off (0x893c2b6c's cases) and on (0x893c3e8c's), by
// rom.4's address; past 10 the first installs none, the second NOISE's (code 8, its float too).  v248's, here from v337
// for the renders' own lookup (Vc2OscAnalogRender.cpp)
inline constexpr std::array<uint32_t, kVc2AnalogCodes> kRenders { 0x893c3890, 0x893c3580, 0x893c5680, 0x893c5500,
                                                                  0x893c5f20, 0x893c5260, 0x893c5a90, 0x893c5810,
                                                                  0x893c2230, 0x893c4fa0, 0x893c4c50 };
inline constexpr std::array<uint32_t, kVc2AnalogCodes> kSubRenders { 0x893c3890, 0x893c3580, 0x893c5990, 0x893c5dc0,
                                                                     0x893c60e0, 0x893c53c0, 0x893c5c90, 0x893c4ac0,
                                                                     0x893c2230, 0x893c50d0, 0x893c4db0 };
inline constexpr uint32_t kSubPast = 8;
// v337: a render routine's waveform code and sub oscillator's switch, as an install picked it: the first table's by
// its code (the sub's HQ-SAW, HQ-SQUARE and NOISE are those), else the second's with the switch on; none for another
struct Vc2AnalogRoutine {
    int code = 0;
    bool sub = false;
};
std::optional<Vc2AnalogRoutine> routineAt(uint32_t at);

// [FW] the block's words it reads: the waveform's code, Impact, the Slope, the sub oscillator's switch (a float, 1 on),
// the LA Tone and its keyfollow's value; Fat and the Pulse Width, each with its envelope's and LFO's depths (the
// tail's envelopes 1 and 2, LFOs 5 and 6)
inline constexpr uint32_t kCode = 0x104, kImpact = 0x130, kSlope = 0x134, kSubOn = 0x138, kTone = 0x140;
inline constexpr uint32_t kToneKf = 0x144, kFat = 0x108, kPw = 0x11c;
inline constexpr Vc2OscTargets::Mod kFatMod { 1, 0x10c, 5, 0x114 }, kPwMod { 2, 0x120, 6, 0x124 };
// [FW] the object's words: the ratio, the gain's target and value, the code's copy, Fat's and the Pulse Width's
// targets and values, the decay, the phase, Impact, the flags, SQUARE's edge, HQ's integrator and direction, LA's
// step, a word nothing writes (rom.2's +0x20c), the second phase, LA's steepness' target and value, NOISE's hold
inline constexpr uint32_t kRatio = 0x1cc, kGainTarget = 0x1dc, kGain = 0x1e0, kCodeCopy = 0x248, kFatTarget = 0x24c;
inline constexpr uint32_t kFatNow = 0x250, kPwTarget = 0x254, kPwNow = 0x258, kDecay = 0x25c, kPhase = 0x260;
inline constexpr uint32_t kImpactNow = 0x264, kFlags = 0x268, kEdge = 0x26c, kHq = 0x270, kHqFalling = 0x274;
inline constexpr uint32_t kLaStep = 0x280, kUnwritten = 0x298, kPhase2 = 0x2a4, kSteepTarget = 0x2b0;
inline constexpr uint32_t kSteepNow = 0x2b4, kNoiseHold = 0x2d4;
// [FW] the sub oscillator's (v244): the second wave's Pulse Width target and value, the level's, the second phase,
// RAMP's last segment, a second word nothing writes (rom.2's +0x214), LA's third phase, the second wave's
// steepness (LA's), SQUARE's and JUNO's steepness of each wave
inline constexpr uint32_t kSwTarget = 0x278, kSwNow = 0x27c, kLevelTarget = 0x28c, kLevelNow = 0x290;
inline constexpr uint32_t kSubPhase = 0x288, kRampEnd = 0x294, kUnwritten2 = 0x2a0, kPhase3 = 0x2a8;
inline constexpr uint32_t kSteep2Target = 0x2b8, kSteep2Now = 0x2bc, kSquareSteep = 0x2c0, kSquareSteep2 = 0x2c8;
// [FW] the sub oscillator's shape's (v246): the block's sub level; the second wave's phase step (beside LA's step,
// both waves' then), RAMP's second segment
inline constexpr uint32_t kSubLevel = 0x13c, kSubStep = 0x284, kRampEnd2 = 0x29c;

// A shape's sum: the block's word, its envelope × its depth, its LFO × its depth (FMAC, product then sum), held to 0…1
// (a NaN passing); rawSum, not held (the sub oscillator's Fat, its octaves)
float sum(const Vc2OscVoice& v, Vc2OscParts& p, uint32_t base, const Vc2OscTargets::Mod& m);
float rawSum(const Vc2OscVoice& v, Vc2OscParts& p, uint32_t base, const Vc2OscTargets::Mod& m);
// NOISE's hold, 1 + 30·|1 − 2x| samples as a byte, 1 for 0, on a routine's own ½ and 30 (v242's, the shape's)
void holdNoise(Vc2OscVoice& v, float x, float half, float span);
// 0x893c3570 on the object: HQ's integrator and direction 0 (a code's `then`: the rest unread)
void hqClear(Vc2OscVoice& v, float, const Vc2OscAnalogPool&);
// 0x893c4a20: x held within m…1 − m, by which side of the half it is on; withinBy on a half of the caller's (the
// shape writes it inline, on its own pool's ½)
float within(float x, float m, const Vc2OscAnalogPool& t);
float withinBy(float x, float m, float half);
// 0x893c2404…: LA-SQUARE's and LA-SAW's way, by the code (9, 10); laTargets its targets on a routine's own words, the
// note-on's and the update's (0x893c31d0, v248)
void laNoteOn(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s, uint32_t code);
void laTargets(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s, uint32_t code, const Vc2OscLaWords& t);
// The plain codes' (0…8) Pulse Width target from x by the code's way, then SQUARE's edge or NOISE's hold, on a
// routine's own words (the note-on's and the update's, v248); past 8 nothing
void codeTargets(Vc2OscVoice& v, float x, uint32_t code, const Vc2OscCodeWords& t);
// 0x893c3b90: the sub oscillator's note-on (v244, Vc2OscAnalogSub.cpp)
void subNoteOn(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s);
// The shape's inputs, single as the SH-4 computes them: the ratio × 2, the second wave's pitch ratio (2^Fat's sum),
// the Pulse Width's sum
struct Vc2OscShapeIn {
    float r, ratio2, w;   // [FW]
};
// 0x893c4020: its shape (v246, Vc2OscAnalogShape.cpp); LA-SQUARE's and LA-SAW's way (Vc2OscAnalogLa.cpp); the phases'
// steps, both waves', on a case's step per ratio
void subShape(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s);
void shapeLaSquare(Vc2OscVoice& v, const Vc2ToneShared& s, const Vc2OscShapeIn& in);
void shapeLaSaw(Vc2OscVoice& v, const Vc2ToneShared& s, const Vc2OscShapeIn& in);
void shapeSteps(Vc2OscVoice& v, const Vc2OscShapeIn& in, float step);
} // namespace Vc2OscAnalog
