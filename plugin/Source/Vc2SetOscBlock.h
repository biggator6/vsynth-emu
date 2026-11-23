#pragma once
// The VC-2 part's conversion sets' OSC blocks (v391, item 8's seventh child): rom.4's 48 of 0x108 bytes from
// 0x89e34060 word for word, two a voice (0x8939fdd0), each an OSC converter's input (v251's layout) converting an OSC
// block of the note's input (W + 0x434, + 0x53c).  The constructor 0x8939fe00: the part, kind and OSC, the voice's key
// and velocity, two bytes of the make's legato struct, the OSC's record from the part's table, the OSC's step count
// (counted on), the coarse and fine tunes with Random Pitch, the key a portamento starts from, then the kind's start and
// its converter (Vc2SetOscKinds).  The conversion 0x893a0040: the voice's bytes, the tunes with a new Random Pitch, the
// start key again, then the converter.  The update 0x893a0250: the record and the tunes again, Random Pitch kept.  The
// copy 0x893a0310: the converter.  The refresh 0x893a0150: whether the record's type or its wave has moved.  The end
// 0x893a0000 is a bare return.  Random Pitch's seed is rom.4's halfword at 0x89996f4c, 31415 from its static
// initializer 0x893a0340, the pool's own word here.  The part's table, the step counts, the voice's glide key, the
// legato struct and M's bend through Vc2SetReads.  Bit for bit: xt_vc2sets' eighth part.  HARDWARE_FIRMWARE.md, "The
// VC-2 part's sets' OSC blocks".  The notes' task.
#include "Vc2NoteMatrix.h"
#include "Vc2PoolOf.h"
#include <array>
#include <cstddef>

struct Vc2SetOscInput {
    uint8_t part;                        // +0x00: the call's part
    uint8_t kind;                        // +0x01: the constructor's kind, its byte
    uint8_t osc;                         // +0x02: its index, the OSC
    uint8_t key;                         // +0x03: the voice's +0x41
    uint8_t velocity;                    // +0x04: its +0x43
    uint8_t legatoKey;                   // +0x05: the make's legato struct's +2
    uint8_t legato;                      // +0x06: and its +1
    uint8_t unread07;
    std::array<uint8_t, 0x80> record;    // +0x08: the OSC's record in the part's table (+8 its switch, +9 its type)
    int16_t coarse;                      // +0x88: the systems', the part's and the record's coarse tunes
    std::array<uint8_t, 2> unread8a;
    int32_t random;                      // +0x8c: Random Pitch, tenths of a cent
    int32_t fine;                        // +0x90: the scale tune, the tunes, the record's fine, M's bend, Random Pitch
    int32_t pitch10, keyPitch10;         // +0x94, +0x98: the converter's
    uint8_t startKey;                    // +0x9c: the key a portamento starts from
    std::array<uint8_t, 0x63> wave;      // +0x9d…+0xff: PCM's start's from +0xa0 (its wave's words)
    uint32_t matrix;                     // +0x100: the note's M
    uint16_t step;                       // +0x104: the OSC's step count when it was made
    std::array<uint8_t, 2> unread106;
};
static_assert(sizeof(Vc2SetOscInput) == 0x108 && offsetof(Vc2SetOscInput, coarse) == 0x88, "rom.4's layout");
static_assert(offsetof(Vc2SetOscInput, startKey) == 0x9c && offsetof(Vc2SetOscInput, matrix) == 0x100, "rom.4's");

// [FW] the pool at 0x89e34060, two a voice
class Vc2SetOscBlocks : public Vc2PoolOf<Vc2SetOscInput, 48, 0x89e34060> {
public:
    static constexpr uint32_t kPerVoice = 2;
    // [FW] 0x8939fdd0(0x108, number, index)
    uint32_t placeOf(uint32_t number, uint32_t index) const { return addressOf(kPerVoice * number + index); }
    void construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads);                // 0x8939fe00
    void convert(uint32_t object, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads);    // 0x893a0040
    void update(uint32_t object, const Vc2SetCall& call, Vc2SetReads& reads);                     // 0x893a0250
    void copy(uint32_t object, uint32_t* block, Vc2SetReads& reads);                              // 0x893a0310
    uint32_t refresh(uint32_t object, const Vc2SetCall& call, Vc2SetReads& reads);                // 0x893a0150

    // [FW] Random Pitch's seed, as 0x893a0340 leaves it, and the halfword after it, which nothing writes
    static constexpr uint32_t kSeedAt = 0x89996f4c;
    struct Seed {
        uint16_t value = 31415;
        uint16_t unread = 0;
    } seed {};
};
