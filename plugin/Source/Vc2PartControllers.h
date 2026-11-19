#pragma once
// The VC-2 part's controllers (v301, step 4.3's twenty-ninth version, item 10's sixth part): rom.4's controller
// 0x89392f90 (the number's object's virtual, then each node on the source's channel's list through the part's
// controller) and the part's controller 0x89394440 (Time Trip Assign X and Y on the part's trip object, Reset All
// Controllers its rest).  Not ported, as nothing in rom.4 calls it: 0x893943c0, a part's trip at rest alone.  The
// number's objects (0x899891e0, the part's init's), a node's word's first byte, the part table 0x8993d040 and the trip
// object's 0x893b0610 and 0x893b0460 through Vc2ControllerParts.  Not in the sound yet.  Bit for bit: xt_vc2settings'
// fifteenth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's controllers".  The notes' task.
#include "Vc2PartSettings.h"
#include <array>
#include <cstddef>
#include <cstdint>

// one-off: rom.4's own layout, checked by its offsets; a declaration, not a job to template
// [FW] a part's trip object's front, the part table's (v245's), as the part's controller reads and writes it
struct Vc2TripFront {
    std::array<uint32_t, 2> unread0 {};   // +0x00
    uint32_t moving = 0;                  // +0x08: the trip under way
    std::array<uint8_t, 0x14> unread0c {};
    int8_t x = 0, y = 0;                  // +0x20, +0x21: the pad's X and Y
    std::array<uint8_t, 2> unread22 {};
    uint8_t level = 0;                    // +0x24: the part's byte the Matrix reads, 127 at rest
    std::array<uint8_t, 3> unread25 {};
};
static_assert(offsetof(Vc2TripFront, x) == 0x20 && sizeof(Vc2TripFront) == 0x28, "rom.4's layout");

// What the controllers reach: the number's object's virtual (its vtable's +0x14, given the source, the channel and,
// from v302, the value, which 0x89392f90 leaves in r7 for it; v301 left it out), a node's word's first byte (its part),
// a part's trip object by the part table, and the trip object's two routines
class Vc2ControllerParts {
public:
    virtual ~Vc2ControllerParts() = default;
    virtual void numberChanged(int32_t number, int source, int32_t channel, int32_t value) = 0;   // [0x899891e0 + 4n]
    virtual uint8_t firstByte(uint32_t word) = 0;
    virtual uint32_t tripAt(uint8_t part) = 0;                                    // [0x8993d040 + 4 part]
    virtual Vc2TripFront& trip(uint32_t address) = 0;
    virtual void tripSet(uint32_t trip, int32_t on, int8_t x, int8_t y) = 0;     // 0x893b0610
    virtual uint8_t tripMove(uint32_t trip, int8_t x, int8_t y) = 0;             // 0x893b0460
};

class Vc2PartControllers {
public:
    Vc2PartControllers(const Vc2PartRows& rows, const Vc2PartNodes& nodes, Vc2ControllerParts& parts)
        : rows_(rows), nodes_(nodes), parts_(parts) {}
    void controller(int source, int32_t channel, int32_t number, int32_t value);   // 0x89392f90
    void partController(uint8_t part, int32_t number, int32_t value);              // 0x89394440

private:
    using Way = void (Vc2PartControllers::*)(uint8_t, int32_t);
    void padX(uint8_t part, int32_t value);
    void padY(uint8_t part, int32_t value);
    void rest(uint8_t part, int32_t value);
    const Vc2PartRows& rows_;
    const Vc2PartNodes& nodes_;
    Vc2ControllerParts& parts_;
};
