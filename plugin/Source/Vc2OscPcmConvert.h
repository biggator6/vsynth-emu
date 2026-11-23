#pragma once
// PCM's OSC converter's own part (v394): rom.4's 0x8939e300, OSC Type 1's in 0x8939fbd0's switch, to the call by the
// wave's type.  Of v251's head it writes the ratio (+0xc), the pitch envelope's and LFO's depths and words, the level and
// its words and the level LFO's depth, alike, and no key, cents or pitch10s.  Its own words: the Time (+0x10, 0x8939c310
// on the record's Time bytes, the OSC group's Time sum, M's +0x180 and +0x181 and the key) with its envelope's depth and
// word (+0x24, +0x28) and LFO's (+0x44, +0x48), both by a curve of three lines; the Formant (+0x14, 0x8939c410) with its
// envelope's (+0x2c, +0x30) and LFO's (+0x4c, +0x50); and the gain (+0x5c), 2^(Wave Gain/6) between the level curve's
// words at the wave's two bytes +0xa4 and +0xa6.  What it calls after, by the wave's type (+0xa7), is LITE's
// 0x8939efa0 or VariPhrase's 0x8939f2a0, not ported yet (Vc2SetOscKinds).  Bit for bit: xt_vc2sets' eighth part.
// HARDWARE_FIRMWARE.md, "The VC-2's PCM converter".  Every address is rom.4's.
#include "Vc2OscConvert.h"
#include <cstdint>

// What the Time law takes: the record's Time depth and keyfollow, the OSC group's Time sum, the key from 60, M's bytes
// +0x180 and +0x181, and the sign the record's Time Mode gives
struct Vc2OscPcmTimeIn {
    int16_t depth = 0, sum = 0, follow = 0, key = 0;
    std::array<uint8_t, 2> beam {};
    float sign = 0;
};
// 0x8939c310, the Time: the depth with the Matrix's sum on a curve of three lines by the sign, plus M's two bytes, ×
// 2^(key × keyfollow ÷ 1200), held to ±5
float vc2OscPcmTime(const Vc2OscPcmTimeIn& in, const Vc2ToneShared& shared);
// 0x8939c410, the Formant: the formant laws' tail (vc2OscFormantHeld) on key × keyfollow ÷ 1200
float vc2OscPcmFormant(int16_t depth, int16_t sum, int16_t follow, int16_t key);

// 0x8939e300 to its call by the wave's type, on the input and the block (0x108 bytes)
void vc2OscPcmConvert(uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared, uint32_t* block);
