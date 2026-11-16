#pragma once
// The VC-2's pitch tracker's constants from rom.4 (v223): its routines' pool words, read once by loadVc2 at the start
// (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "The pitch tracker's frame".
#include <cstdint>

// A candidate period's limits, in samples
struct Vc2TrackerLimits {
    float longest, shortest;
};

// What the table setter 0x893ef240 writes, as the state lays it out from 0x899d9b78
struct Vc2TrackerSettings {
    Vc2TrackerLimits mode0;      // b78: 678.46 and 29.4 (65 to 1500 Hz)
    Vc2TrackerLimits follow;     // b80: 441 and 73.5, the periods the band-pass follows
    Vc2TrackerLimits mode1;      // b88: 2205 and 220.5
    Vc2TrackerLimits mode2;      // b90: 441 and 88.2
    Vc2TrackerLimits mode3;      // b98: 147 and 17.64
    int32_t frameLength;         // ba0: 881, an immediate
    float periods[2];            // ba4, ba8: 63 and 245 samples, the estimator's
    float crossingLevel;         // bac: 50/32768, what a raw crossing and the recent peak must pass
    float levels[5];             // bb0…bc0: 200, 400, 600, 1000 and 3000 over 32768, the estimator's
    float onsetLevel;            // bc4: 0.005, an onset's peak
};
static_assert(sizeof(Vc2TrackerSettings) == 0x50, "rom.4's layout, 0x899d9b78…0x899d9bc8");

// A mode's band-pass and start, as the mode setter 0x893ee100 takes them from its pool: f as 2πF/44100
struct Vc2TrackerMode {
    float highpass, lowpass, period;
};

// A crossing routine's own pool (v224), the up one's from 0x893ee654, the down one's from 0x893ee8dc: the same numbers
struct Vc2TrackerCrossingPool {
    float decay;      // 0.68: a threshold's and a pending peak's fall
    float twoPi;      // 2π: the band-pass's f from a period
    float band;       // 2π·25/44100: the low-pass's target above f0
    float half;       // 0.5: the high-pass's target, f0/2
    float history;    // 1.7: how far the recent peaks may stand above a same-direction crossing's
};

// The onset check's (0x893eea50): an onset's peak over the recent ones, the "too soon" fractions of P and of the last
// candidate, and the late fractions of P a failed crossing passes by, the recent peaks loud or not
struct Vc2TrackerOnsetPool {
    float burst, soon, candidate, failedLoud, failedQuiet;   // 2.3, 0.7, 0.8, 0.85, 0.9
};

// The estimator's (0x893eeb90), by the test each serves
struct Vc2TrackerEstimatorPool {
    float loudFrame;                // 3: the last frame's peak over the recent ones that refuses, from 1000/32768 up
    float lockedAbove, lockedBelow; // 1.07, 0.96: a locked candidate near P
    float handAbove, handBelow;     // 1.05, 0.97: the last candidate nearer still, which is taken instead
    float start;                    // 0.8: how far the half-intervals may disagree, from the start
    float side[2];                  // 0.9, 0.9: the other side's second peak over this one's last, up's and down's words
    float loose;                    // 0.9: the disagreement allowed a far or noisy candidate; the sides' test's
    float off;                      // 0.85: the disagreement allowed a candidate off P
    float above, below;             // 1.5, 0.75: off P
    float underHalf;                // 0.5: a candidate under half P
    float nearAbove, nearBelow;     // 1.8, 0.6: near enough P to skip the sides' test
    float unlockedAbove, agree;     // 1.3, 0.85: unlocked, a candidate within these of P; locked long, the agreement
};

// Taking an estimate's (0x893ef110): agreeing with the last, within these; far from P; the smoothing's half
struct Vc2TrackerTakePool {
    float agreeBelow, agreeAbove, far, wide, half;   // 0.9, 1.1, 0.25, 1.5, 0.5
};

struct Vc2TrackerTables {
    float rate[2] = {};             // the static initializer 0x893edf90's: 44100/440 and 880/44100
    Vc2TrackerSettings settings {}; // the table setter's
    Vc2TrackerMode modes[4] = {};   // modes 0…3: 60/400 Hz from 220.5, 20/100 Hz from 551.25, 100/300 from 441, 200/800 from 88.2
    float highpassStart = 0;        // the reset's high-pass f, 20 Hz (0x893ee3d0)
    float lowpassStart = 0;         // its low-pass f, 170 Hz (0x893ee430)
    float glide = 0;                // the coefficients' glide toward their targets, a sample (0x893ee444)
    float half = 0;                 // the noise count's ½ (0x893ee62c)
    Vc2TrackerCrossingPool crossing[2] = {};   // the analysis's (v224): up's, down's
    Vc2TrackerOnsetPool onset {};
    Vc2TrackerEstimatorPool estimator {};
    Vc2TrackerTakePool take {};
};

[[nodiscard]] bool vc2LoadTracker(const char* path);
bool vc2TrackerLoaded();
const Vc2TrackerTables& vc2TrackerTables();
