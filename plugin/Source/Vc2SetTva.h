#pragma once
// The VC-2 part's conversion sets' TVA (v285, step 4.3's thirteenth version, item 8's fifth child): rom.4's 24 of 40
// bytes from 0x89997c04 word for word, one a voice (0x893a67d0), each converting the TVA block of the note's input
// (W + 0x6e8).  The constructor 0x893a67f0 and the conversion 0x893a68c0 take the velocity curve's level, squared,
// then the block writer 0x893a6260 (the copy 0x893a69a0 its code again); the update 0x893a6940 is the pool's
// (Vc2SetChildHead).  The writer: the level by its Matrix sum and key follow, with the part's CC 7 and 11 when the part
// says so; two depths; the pan by its sum, key follow and CC 10; the outputs by a route; two sends, one with CC 93; the
// part's gain by rom.4's powf; and rom.4's global step at 0x8993eb08, the pool's own word here.  Its end 0x893a68b0 is
// a bare return.  The part's words, the curve, M's controllers and the globals through Vc2SetReads; the tables
// Vc2SetEnvelopeTables'.  Bit for bit: xt_vc2sets' sixth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's sets' TVA".
// The notes' task.
#include "Vc2SetChildHead.h"
#include <cstddef>

struct Vc2SetTvaWords {
    uint8_t kind {};                // +0x00: the note's
    uint8_t key {};                 // +0x01: the voice's +0x41
    uint8_t velocity {};            // +0x02: its +0x43
    std::array<uint8_t, 3> unread03 {};
    uint8_t curve {};               // +0x06…+0x1d: the part's twelve words for it: the velocity's curve,
    uint8_t unread07 {};
    int16_t sens {};                // its depth on the level,
    uint8_t on {};                  // whether the level and pan take their sums and controllers,
    uint8_t unread0b {};
    uint8_t level {};               // the level,
    uint8_t unread0d {};
    int16_t levelFollow {};         // its key follow,
    int8_t depthA {};               // the block's +8 depth (signed),
    uint8_t unread11 {};
    uint8_t pan {};                 // the pan,
    uint8_t unread13 {};
    int16_t panFollow {};           // its key follow,
    int8_t depthB {};               // the block's +0x1c depth (signed),
    uint8_t unread17 {};
    uint8_t output {};              // the outputs' level,
    uint8_t sendB {};               // the send with CC 93 (+0x34),
    uint8_t sendA {};               // the send at +0x30,
    uint8_t route {};               // the outputs' route (0…2),
    uint8_t gain {};                // the sends' common level
    uint8_t unread1d {};
    std::array<uint8_t, 2> unread1e {};
    float velocityLevel {};         // +0x20: the velocity curve's level, squared
    uint32_t matrix {};             // +0x24: the note's M
};

// [FW] the pool at 0x89997c04, one a voice, the voice's bytes at +1
class Vc2SetTvas : public Vc2SetChildPool<Vc2SetTvas, Vc2SetTvaWords, 1, 0x89997c04, &Vc2SetReads::tvaOf, 1> {
public:
    // [FW] 0x893a67d0(40, number): the voice's, its index unread
    uint32_t placeOf(uint32_t number, uint32_t) const { return addressOf(number); }
    // 0x893a67f0 on the call's object: its kind and M, the voice's two bytes, the part's words, the level, the block
    void construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads);
    // 0x893a68c0: the voice's two bytes, the level, then the block
    void convert(uint32_t tva, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads);
    // 0x893a6260: the block, and the step
    void blockOf(const Vc2SetTvaWords& w, uint32_t* block, Vc2SetReads& reads);

    static constexpr uint32_t kStepAt = 0x8993eb08;   // [FW]
    uint32_t step = 0;   // rom.4's float at kStepAt, 1 / (the part's rate / 100 × 44.1)
};
static_assert(sizeof(Vc2SetTvaWords) == 40 && offsetof(Vc2SetTvaWords, curve) == 6, "rom.4's layout, C's TVA");
static_assert(offsetof(Vc2SetTvaWords, velocityLevel) == 0x20, "rom.4's layout, C's TVA");
