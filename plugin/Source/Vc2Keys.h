#pragma once
// The VC-2 part's keys (v276, step 4.3's fourth version): rom.4's key tables K, 0x134 bytes each, one by source and
// channel (4 × 16 from 0x89982720) and two of the part's own (0x89988620, which other code sets up, and 0x89988754),
// and the portamento's keys P, 0x24 bytes each (two by source and channel, 128 from 0x89987420, and the own table's at
// 0x89988888), word for word, the links rom.4's addresses.  K: the init by source and channel 0x893920c0 (its channel
// state from rom.4's table), the reset 0x89392040, the static initializer 0x89392190, the note-on 0x89391e90 and the
// note-off 0x89391ef0 (each key's note-ons and note-offs counted, the key held until they are as many), the Hold
// 0x89391f60 and Sostenuto 0x89391f70 pedals, the clear 0x89391fe0 and the velocity's factor 0x89392280; the held
// keys' queries on a bitmap, 0x89392670 (the highest in a range), 0x893925e0, 0x89392630, 0x893925a0 and 0x893925b0.
// P is rom.2's XtKeyState in layout and laws (XtKeys.h): its note-on 0x893922d0 and note-off 0x89392360 go through it;
// the constructor 0x893922b0 in place, the reset 0x893923c0, the static initializer 0x89392410 and the array's
// 0x89392560.  Bit for bit: xt_vc2keys against rom.4's routines on its own .bss.  HARDWARE_FIRMWARE.md, "The VC-2
// part's keys".  The notes' task.
#include <array>
#include <cstdint>

using Vc2KeyBits = std::array<uint16_t, 8>;   // the 128 keys, key k at bit k & 15 of halfword k >> 4

// A key table K as rom.4 lays it out
struct Vc2KeyTable {
    uint32_t channel = 0;                 // +0x000: the channel's state, by address (rom.4's table's word + 2)
    uint8_t source = 0;                   // +0x004: the source it was set up for
    uint8_t number = 0;                   // +0x005: and the channel
    uint8_t curve = 0;                    // +0x006: the velocity curve
    int8_t onKey = 0;                     // +0x007: the last note-on's key
    uint8_t onVelocity = 0;               // +0x008: its velocity through the curve
    int8_t offKey = 0;                    // +0x009: the last note-off's key
    uint8_t offVelocity = 0;              // +0x00a: its velocity as given
    uint8_t unread0b = 0;                 // +0x00b
    Vc2KeyBits held {};                   // +0x00c: the keys held
    std::array<uint8_t, 128> ons {};      // +0x01c: each key's note-ons, held at 255
    std::array<uint8_t, 128> offs {};     // +0x09c: and its note-offs
    uint8_t hold = 0;                     // +0x11c: the Hold pedal's value
    uint8_t sostenuto = 0;                // +0x11d: the Sostenuto pedal's
    std::array<uint8_t, 2> unread11e {};  // +0x11e
    Vc2KeyBits latched {};                // +0x120: the keys held when Sostenuto went down
    uint8_t wasReset = 0;                 // +0x130: 0xff after a reset
    uint8_t lastOn = 0;                   // +0x131: 1 after a note-on, 0 after a note-off
    std::array<uint8_t, 2> unread132 {};  // +0x132

    void init(uint32_t source, uint32_t channel);      // 0x893920c0
    void reset();                                       // 0x89392040
    void noteOn(uint32_t key, uint32_t velocity);       // 0x89391e90
    void noteOff(uint32_t key, uint32_t velocity);      // 0x89391ef0
    void setHold(uint32_t value);                       // 0x89391f60
    void setSostenuto(int32_t value);                   // 0x89391f70
    void clear();                                       // 0x89391fe0
    float velocityFactor(uint32_t velocity) const;      // 0x89392280: the curve's velocity squared over 127²
};
static_assert(sizeof(Vc2KeyTable) == 0x134, "rom.4's layout, a key table");

// The held keys' queries, on any bitmap
int32_t vc2KeysHighestIn(const Vc2KeyBits& bits, int32_t lo, int32_t hi);   // 0x89392670, −1 none
int32_t vc2KeysHighest(const Vc2KeyBits& bits);                             // 0x893925e0: in 0…127
void vc2KeysSet(Vc2KeyBits& bits, int32_t lo, int32_t hi);                  // 0x89392630: lo…hi
void vc2KeysFill(Vc2KeyBits& bits);                                         // 0x893925a0
bool vc2KeysAny(const Vc2KeyBits& bits);                                    // 0x893925b0

// The portamento's keys P as rom.4 lays them out: rom.2's XtKeyState's
struct Vc2KeyGlide {
    uint32_t table = 0;                   // +0x00: its key table, by address
    uint32_t first = 0;                   // +0x04
    uint32_t last = 0;                    // +0x08: the constructor points it at +4
    int8_t key = 0;                       // +0x0c: the sounding key
    std::array<uint8_t, 3> unread0d {};   // +0x0d
    int32_t lastMs = 0;                   // +0x10: the OS clock at the last note-on
    uint8_t window = 0;                   // +0x14: the chord window, ms
    std::array<uint8_t, 5> unread15 {};   // +0x15
    int8_t prevPoly = 0;                  // +0x1a
    uint8_t prevPolyHeld = 0;             // +0x1b
    int8_t lastPoly = 0;                  // +0x1c
    uint8_t lastPolyHeld = 0;             // +0x1d
    uint16_t since = 0;                   // +0x1e: the ms since the note-on before, held at 0xffff
    int8_t prevMono = 0;                  // +0x20
    uint8_t prevMonoHeld = 0;             // +0x21
    int8_t lastMono = 0;                  // +0x22
    uint8_t lastMonoHeld = 0;             // +0x23

    void construct(uint32_t self);                          // 0x893922b0 on a P given
    void reset();                                           // 0x893923c0
    void noteOn(int32_t ms, const Vc2KeyTable& keys);       // 0x893922d0, at the OS clock's ms
    int32_t noteOff(const Vc2KeyTable& keys);               // 0x89392360(P, 0, 127), every caller's range
};
static_assert(sizeof(Vc2KeyGlide) == 0x24, "rom.4's layout, the portamento's keys");

class Vc2Keys {
public:
    static constexpr int kSources = 4, kChannels = 16, kTables = kSources * kChannels, kGlides = 2 * kTables;
    static constexpr uint32_t kTablesAt = 0x89982720, kGlidesAt = 0x89987420;   // [FW] the tables, the glides
    static constexpr uint32_t kSpareAt = 0x89988620, kOwnAt = 0x89988754;       // [FW] the part's own two
    static constexpr uint32_t kOwnGlideAt = 0x89988888;                         // [FW] the own table's glide

    static uint32_t tableAt(uint32_t source, uint32_t channel);   // by source and channel
    Vc2KeyTable& table(uint32_t address);                         // any of the 66, by address
    Vc2KeyGlide& glide(uint32_t address);                         // any of the 129
    void staticInit();                                            // 0x89392190
    void glidesInit();                                            // 0x89392410
    void constructGlides();                                       // 0x89392560
    // 0x893904d0 (v293, given a part object's first byte, 0…15): the two glides of each source in its channel's
    // place reset, source by source
    void resetObjectGlides(uint8_t index);

    std::array<Vc2KeyTable, kTables> tables {};   // 0x89982720: source s, channel c at 0x1340 × s + 0x134 × c
    std::array<Vc2KeyGlide, kGlides> glides {};   // 0x89987420: source s, channel c, n at 0x480 × s + 0x48 × c + 0x24 × n
    Vc2KeyTable spare {};                         // 0x89988620
    Vc2KeyTable own {};                           // 0x89988754
    Vc2KeyGlide ownGlide {};                      // 0x89988888
};
