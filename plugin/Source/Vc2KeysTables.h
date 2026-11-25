#pragma once
// The part's keys' constants from rom.4 (v276): the velocity curves, the held keys' bit and mask tables, the empty
// bitmap, each source's row of channel states and the rows, and the velocity factor's divisor, read once by loadVc2 at
// the start (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "The VC-2 part's keys".
#include <array>
#include <cstdint>

struct Vc2KeysTables {
    static constexpr int kCurves = 8, kRows = 2;
    std::array<uint8_t, kCurves * 128> curves {};   // 0x89535518: 128 velocities a curve
    std::array<uint16_t, 16> masks {};              // 0x89535918: bits 0…n
    std::array<uint16_t, 16> bits {};               // 0x89535938: bit n
    std::array<uint16_t, 8> zero {};                // 0x89535958: the empty bitmap
    std::array<uint8_t, 4> sources {};              // 0x8950d6d9: each source's row
    uint32_t stateBefore = 0;                       // 0x8950d6dc: the word before (v355: row 0's channel -1 reads it)
    std::array<uint32_t, kRows * 16> states {};     // 0x8950d6e0: each row's 16 channel states, by address
    float divisor = 0;                              // 0x89392400: the velocity factor's, 127²
};

[[nodiscard]] bool vc2LoadKeys(const char* path);
bool vc2KeysLoaded();
const Vc2KeysTables& vc2KeysTables();
