#pragma once
// HIRESO's formant map as read from slot 4 (0x893f15d0), in the firmware's order of operations: research/
// VC2_VOCAL_DESIGNER.md, "HIRESO's formant warp, run".  Its constants are rom.4's, read from the machine at run time
// (warpConstants), never written here.  Single precision, under the SH-4's round-to-zero, FMAC as product then sum.
#include "vc2rom.h"
#include <array>

namespace vc2 {

inline constexpr int kWarpBands = 32;

struct WarpConstants {
    std::array<float, kWarpBands> centre{};   // [FW] 0x8994069c (.data): the bands' centres, Hz
    float pivotBase = 0, pivotStep = 0;       // [FW] slot 4's pool, 0x893f18d4: pivot = base + step * p
    float lowNeg = 0, low = 0;                // [FW] 0x893f18e4, 0x893f18e8: the fixed low end, −100 and 100
    float highNeg = 0, high = 0;              // [FW] 0x893f18dc, 0x893f18e0: the fixed high end
    std::array<float, 4> cubic{};             // [FW] 0x893f18ec…0x893f18f8: Hz to a fractional band index, a0…a3
    float under = 0, over = 0, fade = 0;      // [FW] 0x893f18fc, 0x893f1904, 0x893f1900: 0.95, 1.05, 0.05
};

WarpConstants warpConstants(Machine& m);

// The map slot 4 writes for a side whose pivot field holds p, at `ratio`: the analysis band each synthesis band reads
std::array<float, kWarpBands> warpModel(const WarpConstants& k, int p, float ratio);

} // namespace vc2
