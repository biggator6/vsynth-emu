#pragma once
// The VOCODER OSC's growl, as read from the carrier's render (0x893ebfa0, the branch from 0x893ec240): research/
// VC2_VOCAL_DESIGNER.md, "The growl, run".  One sample's saw as the growl leaves it, before × 0.5 and the saw's filter,
// in the firmware's order of operations.  Its constants are rom.4's, read from the machine (growlStateOf), never written
// here.  Single precision, under the SH-4's round-to-zero, FMAC as product then sum.
#include "vc2rom.h"
#include <cstdint>

namespace vc2 {

struct GrowlState {
    float target = 0, step = 0, phase = 0;      // the saw: its step's target and value (+0x08, +0x0c), its phase (+0x10)
    float growl = 0, growlOverPi = 0;           // +0x3c, and it × 1/π (0x893ec4a0)
    float growlPhase = 0, growlStep = 0;        // +0x40, +0x44: the growl oscillator, a phase in [−1, 1)
    int32_t noiseA = 0, noiseB = 0;             // +0x64, +0x68: the noise generator's two words
    float pitchFactor = 0;                      // 0x899d9964, every carrier's step multiplier
    float half = 0, minusTwo = 0, k440 = 0, k50 = 0;          // the render's pool: 0x893ec230, 0x893ec234, 0x893ec4b0, 0x893ec4b4
    float foldAt = 0, foldNeg = 0, three = 0, four = 0;       // the sine's and cosine's: 0x893f32ec, 0x893f3318, 0x893f3310, 0x893f3314
};

// The carrier's state as it stands in the machine, with the constants
GrowlState growlStateOf(Machine& m, uint32_t carrier);

// One sample: steps the state as the render does, and returns the saw the growl branch hands its filter
float growlStep(GrowlState& s);

} // namespace vc2
