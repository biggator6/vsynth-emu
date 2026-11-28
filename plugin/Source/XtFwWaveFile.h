#pragma once
// A wave saved as the XT saves one (v482): rom.2's writer (0x89496590) run in the interpreter on a store entry put in
// rom.2's own table of waves, its blocks as the XT's store holds them (each its own buffer, none where the wave has
// none), its samples through rom.2's own data device, the OS's file calls hooked (XtFwFileOut.h).  What it writes is
// the XT's .wav: RIFF, fmt, then vpfl (the reader's own, built from the other blocks' sizes), vmdl, vpsc, vpno, vplp,
// vpdt, LIST, rdmd and data, each the wave has.  HARDWARE_FIRMWARE.md {#wave-writer-read}, {#wave-writer}.
#include "XtFiles.h"
#include <cstdint>
#include <string>
#include <vector>

// [FW] whether a wave of this type plays through its bands (3 to 5: BACKING and ENSEMBLE), its samples given to the
// writer by their device (0x894a45c0) and not from its data block
inline bool xtWaveHasBands(int type) {
    return type >= 3 && type <= 5;
}

// `blocks` written as the XT writes a wave (a vpfl given is not used: the writer builds its own).  False, and why,
// with no image, no vpsc, a band wave (types 3 to 5, whose samples come through its bands' device: not yet), one that
// doesn't fit the machine, or a write the writer gives up.
bool xtFwWaveFile(const XtWaveBlocks& blocks, std::vector<uint8_t>& file, std::string* why = nullptr);
