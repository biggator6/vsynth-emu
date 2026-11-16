#pragma once
// The TVA's constants from rom.4 (v233): its routines' pool words, read once by loadVc2 at the start (Vc2PoolTables),
// never after.  Each routine has its own copy of a constant, and each is read where its routine reads it.
// HARDWARE_FIRMWARE.md, "The VC-2's TVA".

struct Vc2TvaTables {
    float onShortest = 0;     // 0x893b2398: 0.1, the note-on's ramp in ms with no attack and no next stage
    float onLongest = 0;      // 0x893b239c: 10, the most its ramp takes, ms
    float onPerMs = 0;        // 0x893b23a0: 44.1 a unit low, samples a ms
    float perSample = 0;      // 0x893b2628: 1/44.1, the update's samples into ms
    float upLongest = 0;      // 0x893b262c: 10
    float upShortest = 0;     // 0x893b2630: 0.1, a stage already over
    float upPerMs = 0;        // 0x893b2634: 44.1 a unit low
    float upSamples = 0;      // 0x893b2638: 441, the update's ramp otherwise
    float floor = 0;          // 0x893b263c: 1.526e-5, the end tests' and held's
    float frames = 0;         // 0x893b2840: 32, a render's
    float shortened = 0;      // 0x893b2844: 1/32.5, rounded to nearest (rom.2's is a unit low)
    float renderFloor = 0;    // 0x893b2848: the render's copy of the floor
};

[[nodiscard]] bool vc2LoadTva(const char* path);
bool vc2TvaLoaded();
const Vc2TvaTables& vc2TvaTables();
