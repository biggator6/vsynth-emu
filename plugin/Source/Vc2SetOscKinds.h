#pragma once
// An OSC block's kind (v391, with Vc2SetOscBlock): rom.4's dispatch 0x8939fb40 and the converter 0x8939fbd0 on an OSC
// block's input.  The dispatch: kind 0 with the OSC off; else by the OSC's type, the analog's start 0x8939ced0 (kind 3;
// the VOCODER's too, then kind 5), EXTIN's 0x8939e940 (kind 4) or PCM's 0x8939e120: 0 at +0x68 and +0x6c, 1 at
// +0x70…+0x7c, the clock's tempo ÷ 10 at +0x60 and 0 at +0x64.  The converter, with a kind: the vibrato's words +0x80…
// +0xb0 (Vc2VibratoBlock's from the Type's row of 0x8953ffd0, the rate, the depth with the LFO's Pitch sum, squared,
// × 300, and its level by 0x8939c260), then by the type the analog's converter 0x8939cf70 (v251's, v256's), the
// VOCODER's 0x8939db00 (v251's, v222's), PCM's 0x8939e300 or EXTIN's 0x8939e9e0.  PCM's start and the PCM and EXTIN
// converters aren't ported yet: rom.4's own through Vc2SetReads::unportedOscKind.  The tables Vc2SetEnvelopeTables' and
// Vc2OscConvertTables'.  Bit for bit: xt_vc2sets' eighth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's sets' OSC
// blocks".  The notes' task.
#include <cstdint>

struct Vc2SetOscInput;
class Vc2SetReads;

void vc2SetOscStart(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads);     // 0x8939fb40
void vc2SetOscConvert(Vc2SetOscInput& in, uint32_t* block, Vc2SetReads& reads);   // 0x8939fbd0
