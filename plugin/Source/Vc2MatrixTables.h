#pragma once
// rom.4's Matrix tables (v235): each destination's limit, which the refresh and the getter 0x893a2350 hold a term and
// a sum to, 79 words from 0x89540158 (destination 0, none, then 1…78); and (v245) the sources' curve right after them,
// 0x895401f6: 128 words, 0…8192 by a 7-bit value, and what follows as far as the bipolar reading reaches (385).  One
// run of words, as rom.4 has them: the refresh reads a limit at any destination byte, past 78 the curve's.  And (v247)
// M's set-up's two destination routines, by a patch's destination 0…78, tabulated by running rom.4's own in the
// image, as rom.2's xtFwMatrixTables does: 0x893a30d0's M destination and uses bit, 0x893a36a0's depth by sensitivity
// (one of ten 64-word tables at 0x895403e8…, chosen by a switch); and the levels' scale, 0x893a3064.  Read once by
// loadVc2 at the start (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame", "The
// VC-2's Matrix sources", "The VC-2's Matrix set-up".
#include <cstdint>

// A patch's destination as 0x893a30d0 takes it: M's destination, and the uses word and bit it sets (0 for none)
struct Vc2MatrixDest {
    uint8_t dest;
    uint8_t group;
    uint16_t bit;
};

struct Vc2MatrixTables {
    static constexpr int kDestinations = 79;   // [FW] 0…78: the limits' words before the curve's
    static constexpr int kCurveWords = 386;    // [FW] the curve's 128 and on to a bipolar reading's farthest, 385
    static constexpr int kWords = kDestinations + kCurveWords;
    static constexpr int kSensReach = 128;     // the depths' sensitivities, −128…127: a 7-bit store's −64…63 and more
    uint16_t words[kWords] {};
    Vc2MatrixDest dests[kDestinations] {};
    int16_t depths[kDestinations][2 * kSensReach] {};
    float levelScale = 0.0f;
};

[[nodiscard]] bool vc2LoadMatrix(const char* path);
bool vc2MatrixLoaded();
// A destination's limit, unsigned (extu.w), for any byte 0…255: past 78 the curve's words
int32_t vc2MatrixLimit(int dest);
// The curve's word `i`, unsigned
uint16_t vc2MatrixCurveWord(int i);
// A patch's destination 0…78 as 0x893a30d0 maps it
Vc2MatrixDest vc2MatrixDestOf(int dest);
// 0x893a36a0's depth for a patch's destination 0…78 and a sensitivity within ±kSensReach
int16_t vc2MatrixDepthOf(int dest, int32_t sens);
// The levels' scale (0x893a3064), a group-2 sum's float × it
float vc2MatrixLevelScale();
