#pragma once
// The VC-2 part's controller numbers and its init (v302, step 4.3's thirtieth version, item 10's seventh part): rom.4's
// 128 number objects of 8 bytes from 0x89988cd0 word for word (the number, its class's vtable) and their table
// 0x899891e0, each made by its class (the part's init's switch on the number, as rows), Hold's class making each
// channel's Hold-release byte 1; the controller's virtual (v301's numberChanged) by the object's class, the classes'
// handlers through Vc2NumberParts until they are ported; and the part's init 0x89393370: part 0's four nodes on part
// object 0 (its plain class, its glides reset, no channel, the word 0x8950d7a0 holds), the keys' and the glides'
// static initializers (v276's), every list cleared, then the numbers.  Not in the sound yet.  Bit for bit:
// xt_vc2settings' sixteenth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's controller numbers and init".  The notes'
// task.
#include "Vc2PartNodes.h"
#include "Vc2PartSettings.h"
#include <array>
#include <cstdint>

// one-off: rom.4's own layout, checked by its own size; a declaration, not a job to template
// [FW] a number object
struct Vc2NumberObject {
    uint8_t number = 0;                   // +0x0
    std::array<uint8_t, 3> unread1 {};
    uint32_t vtable = 0;                  // +0x4: its class's
};
static_assert(sizeof(Vc2NumberObject) == 8, "rom.4's layout, a number object");

// A number class's handler, its vtable's +0x14, given the object, the source, the channel and the value
class Vc2NumberParts {
public:
    virtual ~Vc2NumberParts() = default;
    virtual void handle(uint32_t handler, uint32_t object, int source, int32_t channel, int32_t value) = 0;
};

struct Vc2PartNumbers {
    static constexpr uint32_t kObjectsAt = 0x89988cd0, kTableAt = 0x899891e0;   // [FW]
    static constexpr int kNumbers = 128;                                       // [FW]
    std::array<Vc2NumberObject, kNumbers> objects {};   // 0x89988cd0
    std::array<uint32_t, kNumbers> table {};            // 0x899891e0: each number's object, by address

    // The init's loop: each number's object made by its class, its address in the table
    void make(Vc2PartRows& t);
    // The controller's virtual: the number's object's class's handler
    void changed(int32_t number, int source, int32_t channel, int32_t value, Vc2NumberParts& parts) const;
};

// What the part's init reaches
struct Vc2PartInitWorld {
    Vc2PartRows& t;
    Vc2PartNodes& nodes;
    Vc2Keys& keys;
    Vc2PartNumbers& numbers;
};
void vc2PartInit(Vc2PartInitWorld& w);   // 0x89393370
