#pragma once
// The VC-2's LEGACY engine's tables from rom.4 (v211; the analysis's moved out of Vc2LegacyAnalysis.cpp as they stood):
// the analysis's bands' coefficients (five floats a band, at the .data pointer 0x8994024c), its band count (0x89940250)
// and the hold's coefficients (0x89561538); the synthesis's carrier bands' coefficients (0x89940744, five a band) and
// their gains (0x8994071c).  Read once by loadVc2 at the start (Vc2RomTables); before them no band runs.
#include <cstdint>

struct Vc2LegacyTables {
    static constexpr int kSlots = 32;          // the analysis's rows: at most 31 bands, the unvoiced path after them
    static constexpr int kCoefficients = 5;    // a band's
    static constexpr int kHoldEntries = 101;   // the hold table's, 51…100 reached
    static constexpr int kCarrierBands = 10;   // [FW 0x893f265a] the synthesis's band count is a literal
    int count = 0;                             // the analysis's bands
    float coefficients[(kSlots - 1) * kCoefficients] {};
    float hold[kHoldEntries] {};
    float carrier[kCarrierBands * kCoefficients] {};
    float gains[kCarrierBands] {};
};

[[nodiscard]] bool vc2LoadLegacy(const char* path);
bool vc2LegacyLoaded();
const Vc2LegacyTables& vc2LegacyTables();   // all 0 before the load
