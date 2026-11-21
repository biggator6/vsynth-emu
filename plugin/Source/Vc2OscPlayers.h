#pragma once
// What VariPhrase's and LITE's note-ons share (v240): the object's head, Vari off's words and a loop's rate factor,
// in Vc2OscPlayers.cpp beside VariPhrase's note-on (0x893af420); LITE's (0x893c7c30) in Vc2OscLite.cpp.  The offsets
// are Vc2OscPlayers.cpp's, each the voice's.  HARDWARE_FIRMWARE.md, "The VC-2's players' installs and VariPhrase's
// note-on", "The VC-2's LITE note-on".  Audio thread.
#include "Vc2OscVoice.h"

// Vari off's words in the object (the voice's +0x248…+0x288), as a note-on gives them: the range's first and last,
// the forward loop and the backward (each span written beside them), the loops' modes and rate factors
struct Vc2OscVariOff {
    uint32_t start = 0, end = 0;
    uint32_t loopStart = 0, loopEnd = 0;
    uint32_t bwdEnd = 0, bwdStart = 0;
    uint32_t fwdMode = 0, bwdMode = 0;
    float fwdRate = 0, bwdRate = 0;
};

// The object's head: the voice's own +0x108's address at +0x1a0, the block's +0x60 × `tenThousand` through 2^31 at
// +0x1a8, the wave header's +0x1c at +0x1a4
void vc2OscPlayerHead(Vc2OscVoice& v, Vc2OscParts& parts, float tenThousand);
// A range's first and last sample
struct Vc2OscFrames {
    uint32_t first = 0, last = 0;
};
// Vari off's words of a range and the voice's loop record (the block's +0xdc): its loops, its modes by the object's
// 0x01 (else 0) and its rate factors, exp2(rate / `tenths`)
Vc2OscVariOff vc2OscVariOffOf(const Vc2OscVoice& v, Vc2OscParts& parts, const Vc2ToneShared& shared, float tenths,
                              Vc2OscFrames range);
// Those words but the rate factors, from a loop record already read (v324: LITE's settings' type 0 too)
Vc2OscVariOff vc2OscVariOffLoops(const Vc2OscVoice& v, const Vc2OscLoopRecord& loop, Vc2OscFrames range);
// A loop's rate factor: exp2(rate / `tenths`), its rate in tenths of a cent
float vc2OscLoopRate(int16_t rate, const Vc2ToneShared& shared, float tenths);
// `w` into the object's words; and (v324, LITE's settings' type 0) the range with one side, the backward loop and its
// mode or the forward's
void vc2OscSetVariOff(Vc2OscVoice& v, const Vc2OscVariOff& w);
void vc2OscSetVariOffSide(Vc2OscVoice& v, const Vc2OscVariOff& w, bool backward);
// Then the samples' address, 0 and 1 beside it, the position the walk's sign starts from (the last if it's below 0, else
// the first) and its copy; the position
uint32_t vc2OscVariOffStart(Vc2OscVoice& v);
