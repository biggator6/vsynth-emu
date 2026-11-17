#pragma once
// rom.4's Matrix tables (v235): each destination's limit, which the getter 0x893a2350 holds a sum to, 79 words from
// 0x89540158 (destination 0, none, then 1…78); read once by loadVc2 at the start (Vc2RomTables), never after.
// HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame".
#include <cstdint>

struct Vc2MatrixTables {
    static constexpr int kDestinations = 79;   // [FW] 0…78: the table's words before the next table's
    int32_t limits[kDestinations] {};
};

[[nodiscard]] bool vc2LoadMatrix(const char* path);
bool vc2MatrixLoaded();
const Vc2MatrixTables& vc2MatrixTables();
