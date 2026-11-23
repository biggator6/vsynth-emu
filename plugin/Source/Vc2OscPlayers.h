#pragma once
// What VariPhrase's and LITE's note-ons share (v240): the object's head, Vari off's words and a loop's rate factor,
// in Vc2OscPlayers.cpp beside VariPhrase's note-on (0x893af420); LITE's (0x893c7c30) in Vc2OscLite.cpp.  The offsets
// are Vc2OscPlayers.cpp's, each the voice's.  HARDWARE_FIRMWARE.md, "The VC-2's players' installs and VariPhrase's
// note-on", "The VC-2's LITE note-on".  Audio thread.
#include "Vc2OscVoice.h"
#include "XtWord.h"
#include <span>

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
// v328: the word `at` past the address the head names, where it lies: after a note-on the voice's own (+0x108 + at),
// after a legato's take the earlier tone's voice's, whose object the take brings with its head
uint32_t vc2OscHeadWord(const Vc2OscVoice& v, Vc2OscParts& parts, uint32_t at);
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
// v326 (Vc2OscBeat.cpp): a rom.2 port's words from the voice's and back, each row's place in the voice by `place`
// (XtWord.h: XtBeat's rows the voice's own offsets, XtLitePlayer's rom.2's object's)
using Vc2OscPlace = uint32_t (*)(uint32_t at);
void vc2OscWordsFrom(const Vc2OscVoice& v, void* port, std::span<const XtWord> words, Vc2OscPlace place);
void vc2OscWordsTo(Vc2OscVoice& v, const void* port, std::span<const XtWord> words, Vc2OscPlace place);
// v326 (Vc2OscHeads.cpp): LITE's heads' renders, 0x893c6e80 and 0x893c6e8c, on XtLitePlayer
void vc2OscHeads(Vc2OscVoice& v, float* out, Vc2OscParts& parts, bool stereo);
// v332 (Vc2OscHeads.cpp from v326): a word's place in the voice by its place in rom.2's object, rom.4's object at
// +0x15c with its word inserted at +0x94, LITE's heads' and the band players' place
uint32_t vc2OscInRom4(uint32_t at);
// v332 (Vc2OscBands.cpp): the band players' renders, BACKING's 0x893cadc0 and ENSEMBLE's 0x893ca1f0, on XtMbPlayer
void vc2OscBands(Vc2OscVoice& v, float* out, Vc2OscParts& parts, bool ensemble);
// v327 (Vc2OscModeOne.cpp): kind 1's mode 1's render, 0x893c9f60, mono or stereo by the object's byte 0, on the walk
void vc2OscModeOne(Vc2OscVoice& v, float* out, Vc2OscParts& p);
// v328 (Vc2OscModeOneStart.cpp): its set-up, 0x893cc760, which LITE's install runs for a wave of type 1; and (v333) the
// loops' rates it runs, 0x893ccf50, which the band players' set-ups run too
void vc2OscModeOneSetUp(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared);
void vc2OscLoopRates(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared);
// v333 (Vc2OscBandSetUp.cpp): the band players' set-ups, BACKING's 0x893cd1d0 and ENSEMBLE's 0x893cc8f0, which
// VariPhrase's start runs for a wave of type 3 or 5; and (Vc2OscBands.cpp) their seek, 0x893cd440, before the beat's
// reset (Vc2OscBeat.cpp)
void vc2OscBandSetUp(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared, bool ensemble);
void vc2OscSeekBands(Vc2OscVoice& v, Vc2OscParts& p, int32_t at);
// v325: the two seeks with no band player in them, given the sample, before the beat's reset (Vc2OscBeat.cpp): Vari
// off's 0x893c6360 (Vc2OscPlayers.cpp) and LITE's heads' 0x893c6de0 (Vc2OscLite.cpp)
void vc2OscSeekVariOff(Vc2OscVoice& v, int32_t sample);
void vc2OscSeekHeads(Vc2OscVoice& v, int32_t sample);
// Then the samples' address, 0 and 1 beside it, the position the walk's sign starts from (the last if it's below 0, else
// the first) and its copy; the position
uint32_t vc2OscVariOffStart(Vc2OscVoice& v);
