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

// The multi-band encoder for BACKING (3) / ENSEMBLE (5): the encode driver
// 0x89516000 run to completion.  `vpdt` receives the chunk the firmware
// builds (0x89511a20 / 0x89511de0 / 0x89511e90 / 0x89511c10).  Markers:
// depth < 0 gives a start and an end marker (one segment, a sustained note);
// depth 0..127 runs the XT's attack detection at that Encode Depth (module
// init 0x89516400, then 0x895164c0(depth)), as the encode page does.
// `events`, if given, receives the number of events in the vpdt.
bool xtFwEncodeMulti(int type, const int16_t* pcm, long frames, int channels, std::vector<uint8_t>& vpdt,
                     int depth = -1, int* events = nullptr);

// Round to nearest instead of FPSCR.RM (to match Ghidra's P-code emulator).
void xtFwForceNearest(bool on);
