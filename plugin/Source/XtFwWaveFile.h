#pragma once
// A wave saved as the XT saves one (v482): rom.2's writer (0x89496590) run in the interpreter on a store entry put in
// rom.2's own table of waves, its blocks as the XT's store holds them (each its own buffer, none where the wave has
// none), its samples through rom.2's own data device, the OS's file calls hooked (XtFwFileOut.h).  What it writes is
// the XT's .wav: RIFF, fmt, then vpfl (the reader's own, built from the other blocks' sizes), vmdl, vpsc, vpno, vplp,
// vpdt, LIST, rdmd and data, each the wave has.  A band wave's bands (v483) are made first by rom.2's own rebuild
// (0x894a1ef0), as the XT's load makes them, and its samples come through their device.  HARDWARE_FIRMWARE.md
// {#wave-writer-read}, {#wave-writer}, {#wave-writer-bands}.
#include "XtFiles.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// [FW] whether a wave of this type plays through its bands (3 to 5: BACKING and ENSEMBLE), its samples given to the
// writer by their device (0x894a45c0) and not from its data block
inline bool xtWaveHasBands(int type) {
    return type >= 3 && type <= 5;
}

// A wave's loop points as its vplp keeps them, in frames, named as XtWaveFile names them: +4 the forward loop's start,
// +8 its end, +0xc the backward loop's end (its upper point), +0x10 its start (v486)
struct XtWaveLoops {
    long loopStart = 0, loopEnd = 0, bwdEnd = 0, bwdStart = 0;
};
// A Truncate's Edit Start and End, in frames, both kept (v487)
struct XtWaveCut {
    long from = 0, to = 0;
};
// What the session changes of a wave before it's saved, each set on the store's entry as the XT's own screens set it:
// Truncate by rom.2's edit engine (the edit screen's, v487), then the loops by its loop setter (0x894a4e30, the Loop
// FWD and BWD screens', v486), as the XT's screen sets them after a Truncate
struct XtWaveEdits {
    std::optional<XtWaveCut> cut;
    std::optional<XtWaveLoops> loops;
};

// `blocks` written as the XT writes a wave (a vpfl given is not used: the writer builds its own), with `edits` set on
// it first.  False, and why, with no image, no vpsc, one that doesn't fit the machine, a band wave whose bands the
// rebuild refuses, an edit whose setter fails, or a write the writer gives up.
bool xtFwWaveFile(const XtWaveBlocks& blocks, std::vector<uint8_t>& file, std::string* why = nullptr,
                  const XtWaveEdits& edits = {});
