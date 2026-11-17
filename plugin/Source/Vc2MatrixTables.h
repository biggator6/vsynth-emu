#pragma once
// rom.4's Matrix tables (v235): each destination's limit, which the refresh and the getter 0x893a2350 hold a term and
// a sum to, 79 words from 0x89540158 (destination 0, none, then 1…78); and (v245) the sources' curve right after them,
// 0x895401f6: 128 words, 0…8192 by a 7-bit value, and what follows as far as the bipolar reading reaches (385).  One
// run of words, as rom.4 has them: the refresh reads a limit at any destination byte, past 78 the curve's.  Read once
// by loadVc2 at the start (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame",
// "The VC-2's Matrix sources".
#include <cstdint>

struct Vc2MatrixTables {
    static constexpr int kDestinations = 79;   // [FW] 0…78: the limits' words before the curve's
    static constexpr int kCurveWords = 386;    // [FW] the curve's 128 and on to a bipolar reading's farthest, 385
    static constexpr int kWords = kDestinations + kCurveWords;
    uint16_t words[kWords] {};
};

[[nodiscard]] bool vc2LoadMatrix(const char* path);
bool vc2MatrixLoaded();
// A destination's limit, unsigned (extu.w), for any byte 0…255: past 78 the curve's words
int32_t vc2MatrixLimit(int dest);
// The curve's word `i`, unsigned
uint16_t vc2MatrixCurveWord(int i);
