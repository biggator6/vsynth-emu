#pragma once
// Load-time and encode-time V-Synth XT routines run from the user's own
// rom.2 in the SH-4 interpreter (Sh4Interp): the firmware's code itself,
// bit-for-bit, with nothing of it in this source.  Used where a native port
// does not exist (ENSEMBLE's load-time analysis) and to validate the ports.
#include <cstdint>
#include <vector>
#include "XtMultiBandPlayer.h"
#include "XtSoloEncoder.h"

// rom.2 path (research/ghidra_scripts/unpack_firmware.py).  Returns false if
// the image cannot be read.
bool xtFwOpen(const char* romPath);
bool xtFwOpened();

// The SOLO encoder (0x8951dc40 + 0x8951e120), as XtSoloEncoder.cpp ports it.
bool xtFwSoloEncode(const int16_t* x, int n, std::vector<XtRecord>& out);

// The band data the XT builds when it loads a BACKING (3) or ENSEMBLE (5)
// wave (0x89529e90, 0x894a9940).  Each of the six bands the players use
// (loader slots 0, 2..6) is copied with the memory up to the next band, as
// the player sees it.
bool xtFwLoadBands(int type, const int16_t* pcm, long frames, int channels, XtMbBands& out);

// Round to nearest instead of FPSCR.RM (to match Ghidra's P-code emulator).
void xtFwForceNearest(bool on);
