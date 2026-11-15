#pragma once
// The VC-2's HIRESO engine's tables from rom.4 (v213; the engine's other parts add theirs as they come): the 32 bands'
// centres (0x8994069c, .data) and the formant warp's constants, slot 4's pool (0x893f18d4…0x893f1904).  Read once by
// loadVc2 at the start (Vc2RomTables); before them every map is 0.
#include <cstdint>

struct Vc2HiresoTables {
    static constexpr int kBands = 32;
    float centre[kBands] {};                    // the bands' centres, Hz: 100 to 11874
    float pivotBase = 0, pivotStep = 0;         // a side's pivot is base + step × its field: 500 + 172.27·p Hz
    float highNeg = 0, high = 0;                // the warp's fixed high end, 11874 Hz, and its negation
    float lowNeg = 0, low = 0;                  // its fixed low end, 100 Hz, and its negation
    float a0 = 0, a1 = 0, a2 = 0, a3 = 0;       // the cubic from Hz to a fractional band index
    float under = 0, fade = 0, over = 0;        // the blend's bounds and width: 0.95, 0.05, 1.05
};

[[nodiscard]] bool vc2LoadHireso(const char* path);
bool vc2HiresoLoaded();
const Vc2HiresoTables& vc2HiresoTables();
