#pragma once
// The V-Synth XT's SOLO encoder (grain records for the SOLO player), ported
// from the shared vpenc.dll / rom.2 source.  See XtSoloEncoder.cpp.
#include <cstdint>
#include <vector>

struct XtRecord { int start16; int len16; int info; };   // 1/16 sample; info = vpdt record info word

// The encoder's decimation filter needs a 19-entry low-pass table from the
// XT firmware (rom.2, unpacked by research/ghidra_scripts/unpack_firmware.py).
// It is read from the user's own copy, never shipped: returns false if the
// file is missing or does not look right.
bool xtLoadTables(const char* romPath);
bool xtTablesLoaded();

// Encodes a mono 44.1 kHz wave.  Returns the record stream without the
// (256, 1) terminator: contiguous records tiling [0, n·16); empty if the
// tables are not loaded.
std::vector<XtRecord> xtSoloEncode(const int16_t* x, int n);
