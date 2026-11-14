#pragma once
// The splitbytes run's parts (vc2splitbytes.cpp: the cases, reads and pokes; vc2splitlaws.cpp: the laws).  A VC-2 wave
// is played by OSC 1's Wave Number: the wave library (0x893520a0) is answered with the number less 2999, the VC-2 wave
// of that number, as the store holds no V-Synth wave.
#include "vc2oscinput.h"
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace vc2 {

inline constexpr OscNote kSplitNote{kScratch + 0x20000, kScratch + 0x21000, kScratch + 0x22000};
inline constexpr uint32_t kWaveSplitByte = 0xa4, kShiftByte = 0xa5, kMapSplitByte = 0xa6;   // [FW] in the input
inline constexpr uint32_t kOscGain = 0x5c, kPitchCents = 0x08;   // [FW] the block's (rom.2's +0x5c, +0x8)

struct SplitCase {
    int wave;
    const char* why;
    uint8_t key, velocity;
};
// Input bytes and their values, set after the dispatch
using InputPokes = std::initializer_list<std::pair<uint32_t, uint8_t>>;

// OSC 1's record for a VC-2 wave: the Wave Number set and the builder run; false on a fault
bool recordFor(Machine& m, int wave, Bytes& record);
// The dispatch (the fetch inside it), the pokes after it, then the converter twice: the block after; false on a fault
bool runSplitNote(Machine& m, const Bytes& record, const SplitCase& k, InputPokes pokes, Bytes& block);
uint32_t wordAt(const Bytes& b, uint32_t at);
// Block +0x5c against g·T[+0xa4]/32768·T[+0xa6]/32768 (each product rounded toward zero; T rom.4's level curve, g the
// block's word with both at 127) over each byte at every value, the other at 127, and both at one value; false on a fault
bool checkLevelLaw(Machine& m, const SplitCase& k, int& differ);
// Block +0x08 against (the input's pitch +0x94 − (the root key +0xa8 − the shift +0xa5)·1000)/10 within ±6000, as
// the port's xtPitchCents forms it with the shift 0, over every shift byte; false on a fault
bool checkShiftLaw(Machine& m, const SplitCase& k, int& differ);

} // namespace vc2
