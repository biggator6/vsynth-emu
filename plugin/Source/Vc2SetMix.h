#pragma once
// The VC-2 part's conversion sets' mix (v284, step 4.3's twelfth version, item 8's fourth child): rom.4's 24 of 32
// bytes from 0x89996c34 word for word, one a voice (0x8939bd90), each converting the mix block of the note's input
// (W + 0x644).  The constructor 0x8939bdb0, and on the block writer 0x8939bcc0 the pool's conversion 0x8939be40, update
// 0x8939be60 and copy 0x8939bec0 (Vc2SetChildHead): off, 0.7 at the four gains; on, by its type, the level table by a
// byte (1 and 2), the crossfade's two pairs (3) or 1.0 (4), else 0.7.  Its end 0x8939be30 is a bare return.  The part's
// words through Vc2SetReads::mixOf; the tables Vc2SetEnvelopeTables'.  Bit for bit: xt_vc2sets' fifth part.
// HARDWARE_FIRMWARE.md, "The VC-2 part's sets' mix".  The notes' task.
#include "Vc2SetChildHead.h"
#include <cstddef>

struct Vc2MixWords {
    uint8_t kind {};                // +0x00: the note's
    uint8_t key {};                 // +0x01: the voice's +0x41
    uint8_t velocity {};            // +0x02: its +0x43
    std::array<uint8_t, 3> unread03 {};
    uint8_t on {};                  // +0x06…+0x1b: the part's eleven words for it: whether it mixes,
    uint8_t type {};                // its type (1…4),
    uint8_t level {};               // a level by the level table (types 1 and 2),
    uint8_t first {};               // the crossfade's two (type 3)
    uint8_t second {};
    std::array<uint8_t, 17> unread0b {};
    uint32_t matrix {};             // +0x1c: the note's M
};

// [FW] the pool at 0x89996c34, one a voice, the voice's bytes at +1
class Vc2SetMixes : public Vc2SetChildPool<Vc2SetMixes, Vc2MixWords, 1, 0x89996c34, &Vc2SetReads::mixOf, 1> {
public:
    // [FW] 0x8939bd90(32, number): the voice's, its index unread
    uint32_t placeOf(uint32_t number, uint32_t) const { return addressOf(number); }
    // 0x8939bdb0 on the call's object: its kind and M, the voice's two bytes, the part's words, then the block
    void construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads);
    // 0x8939bcc0: the block
    static void blockOf(const Vc2MixWords& w, uint32_t* block, Vc2SetReads& reads);
};
static_assert(sizeof(Vc2MixWords) == 32 && offsetof(Vc2MixWords, on) == 6, "rom.4's layout, C's mix");
