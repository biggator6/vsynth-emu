#pragma once
// The OSC mix's eight render routines by the number its install chooses them (v269), and which of the tone's tail's
// four words past the sync mask hold what: the member pointer's routine (+0x54) and the output gain (+0x58).
#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr std::array<uint32_t, 8> kVc2MixRoutines = {   // [FW] the member pointers at 0x8993ea64
    0x893e4600, 0x893e4680, 0x893e4740, 0x893e4820, 0x893e4250, 0x893e4320, 0x893adb90, 0x893adb30,
};
inline constexpr size_t kVc2MixRoutineWord = 2, kVc2MixGainWord = 3;
