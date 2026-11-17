#pragma once
// The VC-2's COSM converters the port has, by type (v237): the dispatcher's switch (Vc2CosmConvert.h's Vc2CosmTypes)
// as far as it is ported, each type's converter on its block through its own layout.  The Growl (8, v234), the TVF
// (10, v235) and v237's six: the Amp Simulator (3), the Dual Filter (9), the Dynamic TVF (11), the Compressor (12), the
// Limiter (13) and the TB Filter (16).  The types not here are xt_vc2cosm's hooks yet.  HARDWARE_FIRMWARE.md, "The
// VC-2's COSM converters, the first batch".  The tone generator's thread.
#include "Vc2CosmInput.h"
#include <cstdint>

// Type `type`'s converter on `block`, the COSM's 0x48 bytes, which keeps what the converter doesn't write; false, and
// nothing written, for a type the port hasn't
bool vc2CosmPortedConvert(int type, const Vc2CosmInput& in, uint32_t* block, Vc2CosmMatrix& matrix);
