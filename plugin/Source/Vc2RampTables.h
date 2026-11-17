#pragma once
// The pitch ramp's constants from rom.4 (v236): its routines' pool words, each its own, and the tick's table of the
// gain factor's curve, read once by loadVc2 at the start, never after.  HARDWARE_FIRMWARE.md, "The VC-2's portamento".
#include <array>

// The time law's pool, the restart's and the legato update's each their own words
struct Vc2RampTimePool {
    float perOctave = 0;   // 1200: RATE's time per octave of the span, and the restart's cents per octave
    float slow = 0;        // 100: under it the span is held and type 0 eases
    float spanMost = 0;    // 500
    float spanLeast = 0;   // −500
    float fast = 0;        // 50: under it the time is doubled and the ease is whole
    float easeSlope = 0;   // 0.02: the ease's fall from 50 to 100
    float phaseRate = 0;   // [FW] 655360: the step is it over the time
    float phaseEnd = 0;    // [FW] 65536: the step's most, and the glide's end
};

// A row of the factor's curve: base + curve × (x − at)²
struct Vc2RampRow {
    float at = 0, base = 0, curve = 0;
};

struct Vc2RampTables {
    Vc2RampTimePool restart;    // 0x893b0c30…0x893b0d20
    Vc2RampTimePool legato;     // 0x893b0f78…0x893b0f90, its phaseEnd 0x893b0f6c
    float restartPerCent = 0;   // 0x893b0d28: 1/1200, a ratio's cents into exp2
    float factorLeast = 0;      // 0x893b0f60: 5000, the factor's step held to it
    float factorMost = 0;       // 0x893b0f64: 10000
    float builtPhase = 0;       // [FW] 0x893b0f6c: 65536, the constructor's factor phase
    float builtRise = 0;        // 0x893b0f70: 0.25
    float builtFall = 0;        // 0x893b0f74: 0.5
    float factorEnd = 0;        // 0x893b1054: 65536, the factor's phase's end
    float perPhase = 0;         // [FW] 0x893b1058: just under 1/65536
    float rowsPerX = 0;         // 0x893b105c: 8 rows over x
    float factorFloor = 0;      // 0x893b1064: 2^−24
    float factorPerStep = 0;    // 0x893b1068: 1e−4
    float glideEnd = 0;         // 0x893b1210: 65536, the tick's
    float tickPerCent = 0;      // 0x893b1218: 1/1200
    std::array<Vc2RampRow, 8> rows {};   // 0x89546044: the curve, two bumps over x in [0, 1)
};

[[nodiscard]] bool vc2LoadRamp(const char* path);
bool vc2RampLoaded();
const Vc2RampTables& vc2RampTables();
