#pragma once
// The VC-2 part's conversion sets' envelopes (v282, step 4.3's tenth version, item 8's second child): rom.4's 312 of
// 0x1c bytes from 0x89e2543c word for word, 13 a voice (0x8939a620), each converting an envelope block of the note's
// input (W+0x15c + 0x38 × index: the input's envelope blocks 2…14).  The constructor 0x8939a690, the conversion
// 0x8939a790, the update 0x8939a7b0, the copy 0x8939a800 and the release 0x8939a810, on the block writer 0x8939a3d0:
// each time by a note value and the tempo, or by its value with the Matrix's sum (0x89399ff0, 0x8939a130, 0x8939a270)
// through the time curve 0x8939aa40 or its quarter 0x8939ab10, and the key's factor 0x8939a920 by rom.4's powf.  Its end
// 0x8939a780 is a bare return.  The part's words, the clock and the Matrix's sums through Vc2SetReads; the tables
// Vc2SetEnvelopeTables'.  Bit for bit: xt_vc2sets' third part.  HARDWARE_FIRMWARE.md, "The VC-2 part's sets'
// envelopes".  The notes' task.
#include "Vc2SetChildHead.h"
#include <cstddef>

struct Vc2EnvelopeWords {
    uint8_t kind = 0;               // +0x00: the note's
    uint8_t index = 0;              // +0x01: which of the voice's 13
    uint8_t key = 0;                // +0x02: the voice's +0x41
    uint8_t velocity = 0;           // +0x03: its +0x43
    std::array<uint8_t, 2> unread04 {};
    uint8_t on = 0;                 // +0x06…+0x0f: the part's five words for it: whether it runs,
    uint8_t attack = 0;             // its times by value (128 on a note value) and the sustain's level byte,
    uint8_t decay = 0;
    uint8_t sustain = 0;
    uint8_t release = 0;
    int8_t attackSens = 0;          // the velocity's depths on the times,
    int8_t decaySens = 0;
    int8_t releaseSens = 0;
    int16_t keyfollow = 0;          // and the key's, in cents a key
    uint32_t factor = 0;            // +0x10: the key's factor on the times, 16.16 (0x8939a920)
    uint16_t tempo = 0;             // +0x14: the clock's at construction, held to 20…250 as rom.4 holds it
    uint8_t releaseVelocity = 0;    // +0x16: 64 until a release gives it
    uint8_t unread17 = 0;
    uint32_t matrix = 0;            // +0x18: the note's M
};
static_assert(sizeof(Vc2EnvelopeWords) == 0x1c, "rom.4's layout, an envelope of C's");
static_assert(offsetof(Vc2EnvelopeWords, on) == 6, "the part's words where Vc2SetChildHead puts them");

// [FW] the pool at 0x89e2543c, 13 a voice, placed by 0x8939a620(28, number, index); the conversion 0x8939a790, update
// 0x8939a7b0 and copy 0x8939a800 the pool's (Vc2SetChildHead)
class Vc2SetEnvelopes
    : public Vc2SetChildPool<Vc2SetEnvelopes, Vc2EnvelopeWords, 13, 0x89e2543c, &Vc2SetReads::envelopeOf> {
public:
    // 0x8939a690 on the call's object: its kind, index and M, the voice's two bytes, the part's words, the tempo, then
    // the block (the call's block in W, which `block` is)
    void construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads);
    // 0x8939a810: the release's velocity, then the block's release time again
    void release(uint32_t envelope, uint32_t velocity, uint32_t* block, Vc2SetReads& reads);
    // 0x8939a3d0: the block, and the key's factor
    static void blockOf(Vc2EnvelopeWords& w, uint32_t* block, Vc2SetReads& reads);
};
