#pragma once
// The analog OSC's note-on (v242): rom.4's 0x893c22e0, kind 3's, in the analog OSC's code; the kinds' note-on reaches
// it by its row (Vc2OscNoteOn::analog).  Its head and the other codes' way in Vc2OscAnalog.cpp, LA-SQUARE's and
// LA-SAW's in Vc2OscAnalogLa.cpp; the sub oscillator's note-on (0x893c3b90) is rom.4's by address through Vc2OscParts.
// rom.4's words are rom.2's analog voice's (XtAnalog.h) at +0x8c, its block's at +0x30.  Every address is rom.4's;
// every offset the voice's.  HARDWARE_FIRMWARE.md, "The VC-2's analog note-on".  Audio thread.
#include "Vc2OscTables.h"
#include "Vc2OscTargets.h"
#include "Vc2OscVoice.h"

namespace Vc2OscAnalog {
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

// A shape's sum: the block's word, its envelope × its depth, its LFO × its depth (FMAC, product then sum), held to 0…1
// (a NaN passing)
float sum(const Vc2OscVoice& v, Vc2OscParts& p, uint32_t base, const Vc2OscTargets::Mod& m);
// 0x893c4a20: x held within m…1 − m, by which side of the half it is on
float within(float x, float m, const Vc2OscAnalogPool& t);
// 0x893c2404…: LA-SQUARE's and LA-SAW's way, by the code (9, 10)
void laNoteOn(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s, uint32_t code);
} // namespace Vc2OscAnalog
