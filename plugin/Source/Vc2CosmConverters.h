#pragma once
// The VC-2's COSM converters the port has, by type (v237): the dispatcher's switch (Vc2CosmConvert.h's Vc2CosmTypes)
// as far as it is ported, each type's converter on its block through its own layout.  The Growl (8, v234), the TVF
// (10, v235), v237's six: the Amp Simulator (3), the Dual Filter (9), the Dynamic TVF (11), the Compressor (12), the
// Limiter (13) and the TB Filter (16); and v239's five, on rom.4's libm through the shared math: OD/Dist (1), the Wave
// Shaper (2), the Resonator (5), the Frequency Shifter (14) and Lo-Fi (15); and v241's, the two Side Band Filters (6,
// 7): with them every type the switch calls is here, the Speaker Simulator's (4) inline in the dispatcher.
// HARDWARE_FIRMWARE.md, "The VC-2's COSM converters, the first batch", "The VC-2's COSM converters on rom.4's libm",
// "The VC-2's COSM Side Band Filters".  The tone generator's thread.
#include "Vc2CosmInput.h"
#include <cstdint>

class Vc2ToneShared;

// Type `type`'s converter on `block`, the COSM's 0x48 bytes, which keeps what the converter doesn't write; false, and
// nothing written, for a type the port hasn't.  `shared`: rom.4's powf and logf, for the converters that reach them
bool vc2CosmPortedConvert(int type, const Vc2CosmInput& in, uint32_t* block, Vc2CosmMatrix& matrix,
                          const Vc2ToneShared& shared);
