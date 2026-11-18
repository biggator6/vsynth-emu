#pragma once
// The tone's OSC mix's constants from rom.4 (v269): its routines' pool words and its eight output gains, read once by
// loadVc2 at the start (Vc2PoolTables), never after.  HARDWARE_FIRMWARE.md, "The VC-2's OSC mix".
#include <array>

struct Vc2ToneMixTables {
    std::array<float, 8> gains {};   // 0x89542394: the output gain each render installs with
    float settle = 0;                // 0x893e4670: 1/32, a block's move of the four gains toward their targets
    float ramp = 0;                  // 0x893e46f0: a frame's move of a gain toward its target, over a block
    float follower = 0;              // 0x893bf180: the level follower's scale
    float rateSpan = 0;              // 0x893e4318: 0.005, the follower's rate from its setting
    float rateFloor = 0;             // 0x893e431c: 0.0001, its least
    float half = 0;                  // 0x893adc6c: 0.5
    float fullScale = 0;             // 0x893add74: 2^31, the OSCs' full scale
    float delaySpan = 0;             // 0x893e4988: 248, half the delay lines' samples
};

[[nodiscard]] bool vc2LoadToneMix(const char* path);
bool vc2ToneMixLoaded();
const Vc2ToneMixTables& vc2ToneMixTables();
