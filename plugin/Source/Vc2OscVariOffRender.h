#pragma once
// Vari off's renders (v261): rom.4's 0x893c63c0 (mono) and 0x893c66e0 (stereo), the VC-2's own, not rom.2's Vari-off
// player: 32 frames of a wave by linear interpolation at the object's pitch, on the words its note-on leaves.  Bit for
// bit: xt_vc2osc's Vari off part against rom.4's two routines.  HARDWARE_FIRMWARE.md, "The VC-2's Vari off".  Every
// address is rom.4's.  Audio thread.
#include "Vc2OscVoice.h"
#include <cstdint>

// 32 frames into `out` (left, right), on the voice's object.  The object's +0x10c picks the way: 1 a loop, 2 a loop that
// turns, the way back read inverted; 0 writes nothing.  The stereo render folds its two channels at 70% when the
// object's +0x124 isn't 0.
void vc2OscVariOffMono(Vc2OscVoice& v, float* out, const Vc2OscSamples& wave);
void vc2OscVariOffStereo(Vc2OscVoice& v, float* out, const Vc2OscSamples& wave);
