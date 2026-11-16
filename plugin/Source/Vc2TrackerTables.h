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

struct Vc2TrackerTables {
    float rate[2] = {};             // the static initializer 0x893edf90's: 44100/440 and 880/44100
    Vc2TrackerSettings settings {}; // the table setter's
    Vc2TrackerMode modes[4] = {};   // modes 0…3: 60/400 Hz from 220.5, 20/100 Hz from 551.25, 100/300 from 441, 200/800 from 88.2
    float highpassStart = 0;        // the reset's high-pass f, 20 Hz (0x893ee3d0)
    float lowpassStart = 0;         // its low-pass f, 170 Hz (0x893ee430)
    float glide = 0;                // the coefficients' glide toward their targets, a sample (0x893ee444)
    float half = 0;                 // the noise count's ½ (0x893ee62c)
};

[[nodiscard]] bool vc2LoadTracker(const char* path);
bool vc2TrackerLoaded();
const Vc2TrackerTables& vc2TrackerTables();
