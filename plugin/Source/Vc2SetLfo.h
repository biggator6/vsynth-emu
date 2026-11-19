#pragma once
// The VC-2 part's conversion sets' LFOs (v283, step 4.3's eleventh version, item 8's third child): rom.4's 120 of 20
// bytes from 0x899962b0 word for word, 5 a voice (0x8939b690), each converting an LFO block of the note's input
// (W+0x44 + 0x20 × index: the input's LFO blocks 2…6).  The constructor 0x8939b6c0, and on the block writer
// 0x8939b590 the pool's conversion 0x8939b750, update 0x8939b770 and copy 0x8939b7c0 (Vc2SetChildHead): the waveform
// by 0x8939b3b0, the rate by a note value's ticks or by the rate table with the Matrix's sum (0x8939b450), the two
// depths squared, two switches (0x8939b510 and one inline) and the key sync.  Its end 0x8939b740 is a bare return.  The
// part's words and the Matrix's sums through Vc2SetReads; the tables Vc2SetEnvelopeTables'.  Bit for bit: xt_vc2sets'
// fourth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's sets' LFOs".  The notes' task.
#include "Vc2SetChildHead.h"
#include <cstddef>

struct Vc2LfoWords {
    uint8_t kind {};                // +0x00: the note's
    uint8_t index {};               // +0x01: which of the voice's 5
    uint8_t key {};                 // +0x02: the voice's +0x41
    uint8_t velocity {};            // +0x03: its +0x43
    std::array<uint8_t, 2> unread04 {};
    uint8_t wave {};                // +0x06…+0x0f: the part's five words for it: the waveform's byte,
    uint8_t unread07 {};
    int16_t rate {};                // the rate (128 on a note value),
    uint8_t byte0a {};              // a switch the block takes 0…4 of (else 2),
    uint8_t depthA {};              // a depth,
    uint8_t byte0c {};              // a switch the block takes 1…3 of (else 0),
    uint8_t depthB {};              // a depth,
    uint8_t sync {};                // the key sync
    uint8_t unread0f {};
    uint32_t matrix {};             // +0x10: the note's M
};

// [FW] the pool at 0x899962b0, 5 a voice, placed by 0x8939b690(20, number, index)
class Vc2SetLfos : public Vc2SetChildPool<Vc2SetLfos, Vc2LfoWords, 5, 0x899962b0, &Vc2SetReads::lfoOf> {
public:
    // 0x8939b6c0 on the call's object: its kind, index and M, the voice's two bytes, the part's words, then the block
    void construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads);
    // 0x8939b590: the block
    static void blockOf(const Vc2LfoWords& w, uint32_t* block, Vc2SetReads& reads);
};
static_assert(sizeof(Vc2LfoWords) == 20 && offsetof(Vc2LfoWords, wave) == 6, "rom.4's layout, an LFO of C's");
