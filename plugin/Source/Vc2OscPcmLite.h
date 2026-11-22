#pragma once
// LITE's words (v395): rom.4's 0x8939efa0, which PCM's converter (0x8939e300) calls for a wave of type 0 (+0xa7, PCM's
// start's).  It writes the input's two pitch10s (+0x94 and +0x98) and the block's key (+4) as v251's head does, then
// its own words: the cents of each pitch10 against the wave's root key (+8 and +0x68, the root the wave's +0xa8 less its
// +0xa5, ÷ 10, held to ±6000), the play mode (+0xb0: 2 with the record's Vari Sw, +0x5c, else 0), the Start Offset
// (+0x40) held below the wave's length (+0xf8 and +0x100, the length at +0xfc), four switches (+0xe4: Loop Sw, +0x5b,
// or the wave's flag +0xb0 clear; +0xec: Time Trip under way with Time Trip Sw, +0x5f; +0xe8: Tempo Sync Sw, +0x58;
// +0xf0: Time Trip Beat Keep, +0x60), the wave's 48 bytes +0xcc… (+0xb4…+0xe3) and whether its +0xfc is set (+0xf4);
// the record's offsets from rom.4's OSC block reader (Vc2PartSettingsOscBlock.cpp).  Bit for bit: xt_vc2sets' eighth
// part.  HARDWARE_FIRMWARE.md, "The VC-2's LITE words".  Every address is rom.4's.
#include "Vc2OscConvert.h"
#include <cstdint>

// 0x8939efa0 on the input (0x108 bytes) and the block, `tripMoving` the input's part's trip object's +8
void vc2OscPcmLite(uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared, uint32_t tripMoving,
                   uint32_t* block);
