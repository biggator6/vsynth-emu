#pragma once
// The VC-2's free LFOs' manager (v271): rom.4's object at 0x8a0c6d84, 0x8448 bytes word for word.  384 free objects run at
// every control tick, sounding or not, each for a tone id and LFO number, so that a key-unsynced LFO starts from where
// its phase has got to.  Two pairs of lists hold them: taken and spare, and those the tempo clock reaches and the unused
// nodes for that list.  The links are rom.4's addresses.  A free object's laws are rom.2's XtLfoFree, run on rom.4's words.
// Bit for bit: xt_vc2tone against rom.4's own routines.  HARDWARE_FIRMWARE.md, "The VC-2's free LFOs".  Audio thread.
#include <array>
#include <cstddef>
#include <cstdint>

class XtLfoFree;

struct Vc2FreeLfoWords {
    int32_t lfo = 0;            // +0x00: the LFO number that last took it, −1 none
    uint32_t id = 0;            // +0x04: the tone id that last took it, −1 none
    float rate = 0.0f;          // +0x08: Hz, or a note value's clocks
    float step = 0.0f;          // +0x0c: the phase a tick moves, as its last rate left it
    float phase = 0.0f;         // [FW] +0x10 (0x893aca60): single precision, as rom.4 keeps it
    int32_t refs = 0;           // +0x14: the LFOs following it
    int32_t sync = 0;           // +0x18: a note value
    float clockStep = 0.0f;     // +0x1c
    float clockTarget = 0.0f;   // +0x20
    float clockTime = 0.0f;     // +0x24
    int32_t wrapped = 0;        // +0x28
    int32_t clockCount = 0;     // +0x2c
    float tempo = 0.0f;         // +0x30
    uint32_t node = 0;          // +0x34: its node in the taken and spare lists
};
static_assert(sizeof(Vc2FreeLfoWords) == 0x38, "rom.4's layout, a free object's 0x38 bytes");

struct Vc2FreeLfoNode {
    uint32_t object = 0;        // +0x00
    int32_t unread = 0;         // +0x04: −1 from its constructor
    uint32_t next = 0;          // +0x08
    uint32_t prev = 0;          // +0x0c
};
static_assert(sizeof(Vc2FreeLfoNode) == 0x10, "rom.4's layout, a list node's 0x10 bytes");

struct Vc2FreeLfoBank {
    static constexpr int kObjects = 384;
    Vc2FreeLfoNode taken {}, spare {};                      // +0x0000, +0x0010: the lists' heads
    std::array<Vc2FreeLfoNode, kObjects> nodes {};          // +0x0020
    Vc2FreeLfoNode clocked {}, unclocked {};                // +0x1820, +0x1830: the lists' heads
    std::array<Vc2FreeLfoNode, kObjects> clockNodes {};     // +0x1840
    std::array<Vc2FreeLfoWords, kObjects> objects {};       // +0x3040
    int32_t spareCount = 0, takenCount = 0;                 // +0x8440, +0x8444
};
static_assert(sizeof(Vc2FreeLfoBank) == 0x8448, "rom.4's layout, the manager's 0x8448 bytes");

class Vc2FreeLfos {
public:
    static constexpr uint32_t kAt = 0x8a0c6d84;   // [FW] the manager, 0x893b2010
    static constexpr uint32_t kObjectsAt = kAt + offsetof(Vc2FreeLfoBank, objects);

    void construct();                                       // 0x893ad4b0
    uint32_t take(uint32_t id, int32_t lfo);                // 0x893ad640: its own object if spare, else the first; 0 none
    void release(uint32_t free);                            // 0x893ad720: one LFO fewer, spare again at none
    void addClock(uint32_t free, uint32_t id, int32_t lfo);  // 0x893ad780: on the clock's list
    void removeClock(uint32_t id, int32_t lfo);             // 0x893ad800: off it, its note value forgotten
    void setRate(uint32_t free, int32_t sync, float rate, float tempo);   // 0x893acae0, and what 0x893acf10 sets before it
    void tick(int32_t n);                                   // 0x893ad5f0: every object
    void clock(float seconds);                              // 0x893ad870: the clock's list

    // A free object by rom.4's address; an empty pool's 0 reaches a word of no object, as on the XT it reaches the ROM
    Vc2FreeLfoWords& object(uint32_t free);
    const Vc2FreeLfoWords& noObject() const { return none_; }

    Vc2FreeLfoBank w {};

private:
    static XtLfoFree lawOf(const Vc2FreeLfoWords& o);
    static void keep(const XtLfoFree& f, Vc2FreeLfoWords& o);
    static float stepOf(XtLfoFree f);
    Vc2FreeLfoNode& node(uint32_t address);
    uint32_t addressOf(const void* word) const;
    void moveTo(Vc2FreeLfoNode& list, uint32_t at);
    void append(Vc2FreeLfoNode& list, uint32_t at);
    uint32_t onClock(uint32_t id, int32_t lfo);
    Vc2FreeLfoWords none_ {};
};
