#pragma once
// The VC-2's HIRESO rate filters (v214), which take its three groups of bands between the rates fs, fs/2 and fs/4: rom.4's
// 68-byte filter object (its coefficients' pointer, then 16 states) and the routines the analysis and the synthesis run on
// it.  The decimators keep every second output of three cascaded biquads; the interpolators feed each input then 0
// through the same three and a one-pole.  The stereo path's (v218: 0x893eff90, 0x893f0160, 0x893f0240, 0x893f0350) are the
// mono ones on each channel, `channels` 2: the samples interleaved, left then right, and the states too, a channel's at
// every second word from its own.  Bit for bit: xt_vc2hireso against each routine on a filter object in Sh4Interp, its
// outputs and states.  HARDWARE_FIRMWARE.md, "HIRESO's rate filters", "HIRESO's stereo synthesis".  Audio thread.
#include "Vc2HiresoTables.h"

struct Vc2RateFilter {
    const float* c = nullptr;   // its table: a decimator's ten coefficients or an interpolator's eleven (Vc2HiresoTables)
    float s[16] {};             // as rom.4 keeps them from the object's +4: the biquads' 0…5, the one-pole's 6, a delay's 7
                                // (a stereo one's: 0…11 the biquads', 12, 13 the one-poles', 14, 15 the delays')
};

// 0x893efc30's part on an object it is given: its 16 states 0 (its table it leaves)
void vc2RateClear(Vc2RateFilter& f);
// 0x893efd20 (16 outputs, 32 inputs) and 0x893efdb0 (8, 16); stereo 0x893f0160 (8, 16; 0x893f0080, 16 outputs, has no
// caller): two inputs an output, the second's kept
void vc2Decimate(Vc2RateFilter& f, const float* in, float* out, int outputs, int channels = 1);
// 0x893efc80: 32 frames of two words, each frame's (L + R) × 0.5 into `full`, and that decimated to 16 in `out`; stereo
// 0x893eff90: the frames copied into `full` as they are, and decimated to 16 pairs
void vc2DecimateFrames(Vc2RateFilter& f, const float* frames, float* full, float* out, int channels = 1);
// 0x893efef0 (stereo 0x893f0350): 8 inputs to 16 outputs, added into `out`
void vc2InterpolateAdd(Vc2RateFilter& f, const float* in, float* out, int channels = 1);
// 0x893efe40: 16 inputs to the first words of 32 frames, each output plus `full`'s sample before (32 of them, at full
// rate), which state 7 keeps from one sample to the next; stereo 0x893f0240: to both words, `full` 32 pairs
void vc2InterpolateFrames(Vc2RateFilter& f, const float* in, const float* full, float* frames, int channels = 1);
