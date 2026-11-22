#pragma once
// EXTIN's OSC converter (v393): rom.4's 0x8939e9e0, OSC Type 2's in 0x8939fbd0's switch.  Its first part is the OSC
// block's head as the analog's and the VOCODER's write it (vc2OscBlockConvert), but for the gain, 2^(Wave Gain/6) with
// no calibration's.  Then its own words: +0xb0 whether the systems' +0x10203 is 0; with the part's header's +0x1e set,
// kind 7 on the input's kind 0 and nothing more; else kind 6, and the formant (+0x14, 0x8939c490 on the record's
// formant bytes, the OSC group's Formant sum and the block's key, held to ±1), its envelope's depth by a velocity factor
// (+0x2c) and word (+0x30, 3 on OSC 1, 7 on OSC 2), its LFO's depth with the LFO group's Formant sum (+0x4c, held to
// ±63, ÷ 63), 0 at +0x50, and the level curve's word 40 ÷ 32768 at +0x18.  Bit for bit: xt_vc2sets' eighth part, the
// converter rom.4's own on rom.4's side.  HARDWARE_FIRMWARE.md, "The VC-2's EXTIN converter".  Every address is rom.4's.
#include "Vc2OscConvert.h"
#include <cstdint>

// What EXTIN's own part reads past the input and Vc2OscConvertIn: the systems' byte +0x10203 and the input's part's
// header byte +0x1e
struct Vc2OscExtinIn {
    uint8_t system = 0;
    uint8_t header = 0;
};

// 0x8939c490: a formant offset −1…1 by a depth `depth`, the Matrix's sum `sum`, a row `row` of the table (held to its
// last, 10), a keyfollow `follow` and a key `key`
float vc2OscExtinFormant(int16_t depth, int16_t sum, uint8_t row, int16_t follow, int16_t key);

// 0x8939e9e0 on the input and the block (0x108 bytes, its kind at +0, as the start left it)
void vc2OscExtinConvert(uint8_t* input, const Vc2OscConvertIn& in, const Vc2OscExtinIn& x, const Vc2ToneShared& shared,
                        uint32_t* block);
