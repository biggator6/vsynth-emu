#pragma once
// The VC-2 part's conversion sets' common (v281, step 4.3's ninth version, C's first child of item 8): rom.4's 24 of
// 12 bytes from 0x89996f68 word for word, one by a voice's number (0x893a1940), on a kind-0 note only.  The
// constructor 0x893a1960, the conversion 0x893a19c0, the update 0x893a19e0 and the copy 0x893a1a10, which with its twin
// 0x893a1870 writes the common's block in the note's input (W+0xc, Vc2ToneInput::kOscSet): two switches and a level by
// the velocity, by rom.4's integer arithmetic.  Its end 0x893a19b0 is a bare return.  The part's three words through
// Vc2SetReads.  Bit for bit: xt_vc2sets' second part.  HARDWARE_FIRMWARE.md, "The VC-2 part's sets' common".  The
// notes' task.
#include "Vc2NoteMatrix.h"
#include "Vc2PoolOf.h"

struct Vc2CommonWords {
    uint8_t key = 0;                      // +0: the voice's +0x41
    uint8_t velocity = 0;                 // +1: its +0x43
    std::array<uint8_t, 2> unread02 {};
    uint8_t firstSwitch = 0;              // +4, +5: the part's first word's two bytes, into the block's +0 and +0x10
    uint8_t secondSwitch = 0;
    uint8_t level = 0;                    // +6: its second word's low byte, the level, squared
    uint8_t unread07 = 0;                 // its high byte
    int16_t sens = 0;                     // +8: its third word, the velocity's depth on the level, its sign the side
    std::array<uint8_t, 2> unread0a {};
};
static_assert(sizeof(Vc2CommonWords) == 12, "rom.4's layout, a common");

// [FW] the pool at 0x89996f68, its placement 0x893a1940(10, number)
class Vc2SetCommons : public Vc2PoolOf<Vc2CommonWords, 24, 0x89996f68> {
public:
    static constexpr int kBlockWords = 5;

    // 0x893a1960: the voice's two bytes and the part's words, then the block
    void construct(uint32_t common, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads);
    // 0x893a19c0: the voice's two bytes again, then the block
    void convert(uint32_t common, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads);
    // 0x893a19e0: the part's words again
    void update(uint32_t common, const Vc2SetCall& call, Vc2SetReads& reads);
    // 0x893a1a10 and 0x893a1870, the same code: the block from the common's words
    void copy(uint32_t common, uint32_t* block) { blockOfCommon(at(common), block); }
    static void blockOfCommon(const Vc2CommonWords& w, uint32_t* block);

private:
    void fromPart(Vc2CommonWords& w, const Vc2SetCall& call, Vc2SetReads& reads);
    static void fromVoice(Vc2CommonWords& w, const Vc2SetCall& call, Vc2SetReads& reads);
};
