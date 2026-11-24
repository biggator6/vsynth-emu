#pragma once
// The VC-2 part's controller numbers and its init (v302, step 4.3's thirtieth version, item 10's seventh part): rom.4's
// 128 number objects of 8 bytes from 0x89988cd0 word for word (the number, its class's vtable) and their table
// 0x899891e0, each made by its class (the part's init's switch on the number, as rows), Hold's class making each
// channel's Hold-press byte 1; the controller's virtual (v301's numberChanged) by the object's class, and (v303) the
// classes' handlers, each a class row's way (Vc2PartNumbers.cpp), and the glides' held flags of a channel's nodes
// cleared (0x89393000); the listeners of a channel's state, the vocoder's pedal and v278's release path over a key
// table's voices through Vc2NumberParts; and the part's init 0x89393370: part 0's four nodes on part
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

// What the handlers reach beyond the key tables and glides (v303): the listeners of a channel's state, the vocoder's
// pedal, and v278's release path over a key table's voices (Vc2Assigner's), each given the table by address
class Vc2NumberParts {
public:
    virtual ~Vc2NumberParts() = default;
    virtual void notify(uint32_t state) = 0;          // 0x89390ea0: the listeners at [0x899826e0] of that state
    virtual void vocoderHold(int32_t value) = 0;      // 0x893b1c10: Vc2Vocoder::hold (0x893f0770)
    virtual void holdOff(uint32_t table) = 0;         // 0x89390cd0
    virtual void sostenutoOff(uint32_t table) = 0;    // 0x89390d10
    virtual void truncateTable(uint32_t table) = 0;   // 0x89390d50
    virtual void releaseBut(uint32_t table) = 0;      // 0x89390da0
    virtual void releaseAll(uint32_t table) = 0;      // 0x89390e20
};

// What the handlers reach (v303)
struct Vc2NumberWorld {
    Vc2PartRows& t;
    Vc2PartNodes& nodes;
    Vc2Keys& keys;
    Vc2NumberParts& parts;
};

struct Vc2PartNumbers {
    static constexpr uint32_t kObjectsAt = 0x89988cd0, kTableAt = 0x899891e0;   // [FW]
    static constexpr int kNumbers = 128;                                       // [FW]
    std::array<Vc2NumberObject, kNumbers> objects {};   // 0x89988cd0
    std::array<uint32_t, kNumbers> table {};            // 0x899891e0: each number's object, by address

    // The init's loop: each number's object made by its class, its address in the table
    void make(Vc2PartRows& t);
    // The number's object (the table's word) and its class's handler, both by address
    uint32_t objectOf(int32_t number) const;
    uint32_t handlerOf(int32_t number) const;
    // The controller's virtual: the number's object's class's handler, the port's own from v303
    void changed(int32_t number, int source, int32_t channel, int32_t value, Vc2NumberWorld& w) const;
};

// v345: All Sound Off to a source's channel as the mode row makes it: a message of class 7 (0x89393c00, on the stack)
// handed on to its handler 0x89393ba0 with a value of 0
void vc2NumbersSoundOff(int source, int32_t channel, Vc2NumberWorld& w);

// What the part's init reaches
struct Vc2PartInitWorld {
    Vc2PartRows& t;
    Vc2PartNodes& nodes;
    Vc2Keys& keys;
    Vc2PartNumbers& numbers;
};
void vc2PartInit(Vc2PartInitWorld& w);   // 0x89393370
