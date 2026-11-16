#pragma once
// The VC-2's HIRESO rate filters (v214), which take its three groups of bands between the rates fs, fs/2 and fs/4: rom.4's
// 68-byte filter object (its coefficients' pointer, then 16 states) and the routines the analysis and the mono path run on
// it.  The decimators keep every second output of three cascaded biquads; the interpolators feed each input then 0
// through the same three and a one-pole.  The stereo path's four (0x893eff90, 0x893f0160, 0x893f0240, 0x893f0350) come
// with the stereo synthesis.  Bit for bit: xt_vc2hireso against each routine on a filter object in Sh4Interp, its outputs
// and states.  HARDWARE_FIRMWARE.md, "HIRESO's rate filters".  Audio thread.
#include "Vc2HiresoTables.h"

struct Vc2RateFilter {
    const float* c = nullptr;   // its table: a decimator's ten coefficients or an interpolator's eleven (Vc2HiresoTables)
    float s[16] {};             // as rom.4 keeps them from the object's +4: the biquads' 0…5, the one-pole's 6, a delay's 7
};

// 0x893efc30's part on an object it is given: its 16 states 0 (its table it leaves)
void vc2RateClear(Vc2RateFilter& f);
// 0x893efd20 (16 outputs, 32 inputs) and 0x893efdb0 (8, 16): two inputs an output, the second's kept
void vc2Decimate(Vc2RateFilter& f, const float* in, float* out, int outputs);
// 0x893efc80: 32 frames of two words, each frame's (L + R) × 0.5 into `mixed`, and that decimated to 16 in `out`
void vc2DecimateFrames(Vc2RateFilter& f, const float* frames, float* mixed, float* out);
// 0x893efef0: 8 inputs to 16 outputs, added into `out`
void vc2InterpolateAdd(Vc2RateFilter& f, const float* in, float* out);
// 0x893efe40: 16 inputs to the first words of 32 frames, each output plus `full`'s sample before (32 of them, at full
// rate), which state 7 keeps from one sample to the next
void vc2InterpolateFrames(Vc2RateFilter& f, const float* in, const float* full, float* frames);
