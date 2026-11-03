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

// Encodes a 44.1 kHz wave of n frames: mono, or interleaved stereo in the
// firmware's stereo mode.  Returns the record stream without the (256, 1)
// terminator: contiguous records tiling [0, n·16); empty if the tables are
// not loaded, and for the lengths the firmware makes no list for (1, 2, 2004
// and 2005 samples).
// trace, for xt_enc: one hash of the encoder's working memory after each fill, scratch
// and drift, the same as xtFwSoloEncode's of the firmware's.
std::vector<XtRecord> xtSoloEncode(const int16_t* x, int n, int channels = 1, std::vector<uint64_t>* trace = nullptr);

// One entry of such a trace: what the step was, and its memory.
inline uint64_t xtEncHash(int kind, const void* p, size_t bytes) {
    uint64_t h = 14695981039346656037ull + (uint64_t)kind;
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < bytes; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
