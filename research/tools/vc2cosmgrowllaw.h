#pragma once
// COSM type 8, the VC-2's GROWL, as read from rom.4 (research/VC2_VOCAL_DESIGNER.md, "COSM type 8, the Growl, run"):
// the converter's block (0x89398780), the update's depth and divider factor (0x893b9d40) and the DSP (0x893b9b40), in
// the firmware's order of operations.  Its constants are rom.4's, read from the machine (cosmGrowlConstsOf), never written
// here.  Single precision, under the SH-4's round-to-zero, FMAC as product then sum (vc2rom.h).  vc2cosmgrowl.cpp holds
// it to the firmware.
#include "vc2rom.h"
#include <cstdint>
#include <vector>

namespace vc2 {

inline constexpr int kGrowlLine = 3267, kGrowlTapBack = 512, kGrowlFrames = 32;   // [FW] the lines, the tap, a block

// The DSP's pool (0x893b9c80): 3, 15.95, 1e-4, 0.5, 4, the line's length, −2, the pitch factor, −0.5, −the length; the
// update's (0x893b9e94): the speed's floor, 440, 50
struct CosmGrowlConsts {
    float three, fmScale, tiny, half, four, line, minusTwo, rate, minusHalf, minusLine, speedFloor, k440, k50;
};
CosmGrowlConsts cosmGrowlConstsOf(Machine& m);

// The block's fields from Intencity and Speed (0…127, plus any Matrix term), the update's two words, and the DSP's state
// (position, phase, step, the two lines), one 32-frame block at a time.  `plant` drops the divider's odd-position step.
struct CosmGrowl {
    const CosmGrowlConsts& c;
    bool plant = false;
    float depth = 0, factor = 0, phase = 0, step = 0;
    int pos = 0;
    std::vector<float> l, r;

    static float blockIntensity(int intensity);   // block +4: I/127
    static float blockSpeed(int speed);           // block +8: 2·S/128 − 1
    void update(float intensity, float speed);    // depth, and the divider factor 440 / ((s + 1)·50)
    float wave(float x) const;                    // x·(3 − 4x²), the sine both taps are moved by
    void block(float pitch, float* io);           // 32 frames, L and R, in place; pitch is the object's +0
};

} // namespace vc2
