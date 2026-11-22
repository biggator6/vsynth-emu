#pragma once
// The VC-2 part's Time Trip (v306, step 4.3's thirty-fourth version, item 10's eleventh part): rom.4's 16 part records
// of 0x34 bytes at 0x89989408 (the part table 0x8993d040's, v301's), each a trip object's 0x20 bytes, the pad's X and
// Y, the part's level and the rate's index; their static initializer 0x89394570; the trip object's constructor
// 0x893b0400 (in place: the initializer always gives it the object), rate 0x893b05b0, set 0x893b0610 and X-Y move
// 0x893b0460 with the position 0x893b0690 (a polar table's radius and angle) and the steps 0x893b0790 (the turn since
// the last move in the rate's steps, on its scratch words 0x899996a4…0x899996ac) and the steps' broadcast 0x893b1780 to
// each sounding tone's +0x48 (v228's 0x893b4ca0) through Vc2TripParts.  Not ported, as nothing in rom.4 calls them: the
// X-only and Y-only moves 0x893b04d0 and 0x893b0540 and the steps' sender 0x893b0740; nor 0x893b09f0's two floats
// (0x899996b0, 0x899996b4), which nothing reads.  The tables read by Vc2PartSettingsTables.  Not in the sound yet.  Bit
// for bit: xt_vc2settings' nineteenth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's Time Trip".  The notes' task.
#include <array>
#include <cstddef>
#include <cstdint>

// one-off: rom.4's own layout, checked by its offsets; a declaration, not a job to template
// [FW] a trip object, as rom.4 lays it out
struct Vc2TripObject {
    uint32_t part = 0;                    // +0x00: what a tone's +0x1c is matched to (the record's part)
    uint32_t mask = 0;                    // +0x04: and under (0xffff)
    uint32_t moving = 0;                  // +0x08: the trip under way (v301's)
    int8_t turning = 0;                   // +0x0c: the way it turns, 1 or −1, 0 not yet
    uint8_t unread0d = 0;
    int16_t carry = 0;                    // +0x0e: a turn against the way, held for the next
    int16_t offset = 0;                   // +0x10: the rate's offset, a turn back's
    int16_t steps = 0;                    // +0x12: the rate's steps a whole turn (512)
    uint8_t byte14 = 0, byte15 = 0;       // +0x14, +0x15: 64 made, 0 stopped; unread here
    int8_t lastRadius = 0;                // +0x16
    uint8_t unread17 = 0;
    int16_t lastAngle = 0;                // +0x18: the angle at the last move, −1 none
    int8_t x = 0, y = 0;                  // +0x1a, +0x1b
    int8_t radius = 0;                    // +0x1c
    uint8_t unread1d = 0;
    int16_t angle = 0;                    // +0x1e: 0…512, −1 none
};
static_assert(offsetof(Vc2TripObject, angle) == 0x1e && sizeof(Vc2TripObject) == 0x20, "rom.4's layout");

// one-off: rom.4's own layout, checked by its offsets; a declaration, not a job to template
// [FW] a part's record's front, as the part's controller reads and writes it (v301; the trip object named from v306)
struct Vc2TripFront {
    Vc2TripObject trip {};                // +0x00
    int8_t x = 0, y = 0;                  // +0x20, +0x21: the pad's X and Y
    std::array<uint8_t, 2> byte22 {};     // +0x22, +0x23: 0 made; unread here
    uint8_t level = 0;                    // +0x24: the part's byte the Matrix reads, 127 at rest
    std::array<uint8_t, 3> unread25 {};
};
static_assert(offsetof(Vc2TripFront, x) == 0x20 && sizeof(Vc2TripFront) == 0x28, "rom.4's layout");

// one-off: rom.4's own layout, checked by its offsets; a declaration, not a job to template
// [FW] a part's record
struct Vc2TripRecord {
    Vc2TripFront front {};
    int32_t rate = 0;                     // +0x28: the rate's index, 2
    float perRate = 0;                    // +0x2c: 4 over it
    uint8_t part = 0;                     // +0x30
    std::array<uint8_t, 3> unread31 {};
};
static_assert(sizeof(Vc2TripRecord) == 0x34, "rom.4's layout");

// The steps' scratch words, rom.4's globals: the angle from, to, and the turn between
struct Vc2TripScratch {
    int32_t from = 0, to = 0, turn = 0;
};

// What the steps' broadcast reaches: the sounding tones (v273's list) and each one's +0x48
class Vc2TripParts {
public:
    virtual ~Vc2TripParts() = default;
    virtual uint32_t firstTone() = 0;              // [0x899996d0]
    virtual uint32_t nextTone(uint32_t tone) = 0;  // its +0
    // its vtable's (+0x18) +0x48, 0x893b4ca0: its OSC voices' counts moved when its +0x1c under `mask` is `part`
    virtual void stepTone(uint32_t tone, int32_t count, uint32_t part, uint32_t mask) = 0;
};

class Vc2TripRecords {
public:
    static constexpr uint32_t kAt = 0x89989408, kScratchAt = 0x899996a4;   // [FW]
    static constexpr int kParts = 16;
    void staticInit();   // 0x89394570
    std::array<Vc2TripRecord, kParts> records {};
    Vc2TripScratch scratch {};
};

void vc2TripConstruct(Vc2TripObject& t, uint32_t part, uint32_t mask);   // 0x893b0400
void vc2TripRate(Vc2TripObject& t, int32_t index);                       // 0x893b05b0
void vc2TripSet(Vc2TripObject& t, int32_t on, int8_t x, int8_t y);       // 0x893b0610
// 0x893b0460: the radius
int8_t vc2TripMove(Vc2TripObject& t, int8_t x, int8_t y, Vc2TripScratch& s, Vc2TripParts& parts);
void vc2TripPosition(Vc2TripObject& t);                                  // 0x893b0690
void vc2TripPlace(Vc2TripObject& t, int8_t x, int8_t y);   // the move's first part: X and Y stored, the position
int32_t vc2TripSteps(Vc2TripObject& t, Vc2TripScratch& s);               // 0x893b0790
void vc2TripBroadcast(int32_t count, uint32_t part, uint32_t mask, Vc2TripParts& parts);   // 0x893b1780
