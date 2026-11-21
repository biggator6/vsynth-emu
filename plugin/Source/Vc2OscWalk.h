#pragma once
// The players' event walk (v327): rom.4's routines that move a player through its wave's events and loops, on the
// object's words (the voice's +0x15c), the events' table and the loops' record read through the voice's parts.  They
// are SOLO's walk (rom.2's 0x8950c7e0, 0x8950ccc0, 0x8950c880, 0x8950ca70, which XtSoloVoice ports on its own state),
// with the event kinds' ways that the object's byte 2 opens.  Each player passes its index (r5: 0 kind 1's mode 1, 4
// and 8 SOLO's, 12, 16), which picks its two event jumps (the tables at 0x893c9350 and 0x893c9364) and, at 16, keeps
// the step's sign at a turn: a row, Vc2WalkWay, mode 1's alone so far.  HARDWARE_FIRMWARE.md, "The VC-2's mode 1".
#include "Vc2OscVoice.h"
#include <bit>

// The object's words by its own offsets, as the walk and mode 1's render read them, and a word of the events' table
// or the loops' record by its address (a half word its low half), through the voice's parts
struct Vc2WalkWords {
    Vc2OscVoice& v;
    Vc2OscParts& p;
    static uint32_t at(uint32_t off) { return Vc2OscVoice::kObject + off; }
    uint8_t byte(uint32_t off) const { return v.u8(at(off)); }
    void setByte(uint32_t off, uint8_t b) { v.setU8(at(off), b); }
    int16_t half(uint32_t off) const { return (int16_t)v.u16(at(off)); }
    void setHalf(uint32_t off, int32_t h) { v.setU16(at(off), (uint16_t)h); }
    int32_t word(uint32_t off) const { return (int32_t)v.u32(at(off)); }
    void setWord(uint32_t off, int32_t w) { v.setU32(at(off), (uint32_t)w); }
    float real(uint32_t off) const { return v.f32(at(off)); }
    void setReal(uint32_t off, float f) { v.setF32(at(off), f); }
    // a word of the events' table or the loops' record by its address, aligned (on the SH-4 a MOV.L from elsewhere is
    // an address error), and a half word by its address, its word's low or high half (Vc2OscWalk.cpp)
    uint32_t memory(uint32_t address) const { return std::bit_cast<uint32_t>(p.boundFloat(address, 0)); }
    int16_t memoryHalf(uint32_t address) const;
};
// The SH-4's sum of two words, wrapping
inline int32_t vc2WalkPlus(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }

struct Vc2WalkWay {
    void (*jump)(Vc2OscVoice& v, Vc2OscParts& p);                         // to the event +0xc names
    void (*jumpCached)(Vc2OscVoice& v, Vc2OscParts& p, uint32_t cached);   // to a loop's cached event, by its address
    bool keepsSign;                                                       // a turn leaves the step as it is
};
extern const Vc2WalkWay kVc2ModeOneWalk;   // index 0: 0x893ca190, 0x893ca1a0

void vc2WalkCrossing(Vc2OscVoice& v, Vc2OscParts& p);                          // 0x893c9380
void vc2WalkForward(Vc2OscVoice& v, Vc2OscParts& p, const Vc2WalkWay& way);    // 0x893c8f50
void vc2WalkBackward(Vc2OscVoice& v, Vc2OscParts& p, const Vc2WalkWay& way);   // 0x893c9140
