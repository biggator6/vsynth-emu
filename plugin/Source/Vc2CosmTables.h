#pragma once
// The VC-2's COSM constants from rom.4 (v234): the Growl's, its DSP's and its update's pool words; the dispatcher's and
// the TVF's converter's (v235); read once by loadVc2 at the start (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md,
// "The VC-2's COSM type 8, the Growl", "The VC-2's COSM converters' frame".
#include "Vc2Math.h"

// The Growl's: the DSP's pool (0x893b9c80…0x893b9ca7) and the update's and new block's (0x893b9e94…0x893b9e9f)
struct Vc2CosmGrowlPool {
    // The DSP's 0.5, −0.5, 3 and 4, which its two taps' sine and cosine take (vc2Sine, vc2Cosine): its own words,
    // not the shared routines' (0x893b9c8c, 0x893b9ca0, 0x893b9c80, 0x893b9c90)
    Vc2MathPool taps {};
    float fmScale = 0;     // 15.95: the second tap's swing at depth 1 and pitch 1, in samples
    float tiny = 0;        // 1e-4: a step under it starts the phase again
    float line = 0;        // 3267, the lines' length, and its negative
    float minusLine = 0;
    float minusTwo = 0;    // the phase's wrap
    float rate = 0;        // 880/44100: the phase's step a pitch, over the divider
    float speedFloor = 0;  // the update's: Speed held to it…1
    float k440 = 0, k50 = 0;   // the divider factor, 440 / ((s + 1) × 50)
};

// The TVF's converter's (0x89398da0…0x89398db7, v235)
struct Vc2CosmTvfPool {
    float percent = 0;     // 100: the keyfollow's per cent
    float k127 = 0;        // 127: the keyfollow's per step
    float k63 = 0;         // 63: a depth's per step
    float qScale = 0;      // 0.984375: Q's top below 1
};

struct Vc2CosmTables {
    Vc2CosmGrowlPool growl {};
    Vc2CosmTvfPool tvf {};
    float velocityScale = 0;   // 32768: the dispatcher's, a velocity factor's 0…32768 to 0…1 (0x893999ac, v235)
};

[[nodiscard]] bool vc2LoadCosm(const char* path);
bool vc2CosmLoaded();
const Vc2CosmTables& vc2CosmTables();
