#pragma once
// The OSC voice's constants from rom.4 (v230; the targets' from v232): its routines' pool words and the three floats
// its install reads, read once by loadVc2 at the start (Vc2PoolTables), never after.  HARDWARE_FIRMWARE.md, "The
// VC-2's OSC voice", "The VC-2's OSC voice's note-on and targets".

// A pair of bounds as rom.4 holds them, each its routine's own pool word
struct Vc2OscBounds {
    float most = 0, least = 0;
};

// A ratio's pool: the cents' bounds, 1/1200, the ratio's bounds
struct Vc2OscTargetPool {
    Vc2OscBounds cents, ratio;
    float perOctave = 0;
};

struct Vc2OscTables {
    float waitLevel = 0;      // 0x893ae3d0: the update waits while the object's level (+0x84) is above it
    float depthScale = 0;     // 0x893ae3f4: 0.2, the block's +0x1c onto the voice's +0x70 at a note-on
    float plainHz = 0;        // 0x893ae48c: 440, a note-on's +0xa80 with no wave
    float rateTop = 0;        // 0x893ae628: 44000, over the wave header's rate word
    float sampleHz = 0;       // 0x893ae630: 440, a wave's +0xa80 when it isn't pitched from its root
    float cent = 0;           // 0x893ae634: 0.01
    float exponentBase = 0;   // 0x893ae638: −5.75
    float keyCents = 0;       // 0x893ae63c: 100
    float perOctave = 0;      // 0x893ae640: 1/1200
    float tuneScale = 0;      // 0x893ae644: 1/120000, the loop record's tune into the factor's exponent
    float settingsScale = 0;  // 0x893ae87c: 0.2, the settings' +0x1c onto +0x70
    Vc2OscBounds getterCents;   // 0x893aed7c, 0x893aed80: ±6000, the pitch getter's
    float installed4 = 0;     // 0x89542390: the install's +0xa88 for kinds 4 and 7
    float installed5 = 0;     // 0x895422f0: kind 5's
    float installed6 = 0;     // 0x895422f4: kind 6's
    // The targets' (v232)
    Vc2OscBounds cents;         // 0x893afb38, 0x893afb3c: ±6000, the cents' (0x893af9f0)
    Vc2OscTargetPool ratio;     // 0x893afdc0…0x893afdd8: the ratios' (0x893afb40, 0x893afb80, 0x893afd10)
    Vc2OscTargetPool noteOn;    // 0x893af35c…0x893af370: the kinds' note-on's own
    float kind6Least = 0;       // 0x893af530: −32, kind 6's ratio's lower bound there
    Vc2OscBounds walkTarget;    // 0x893afddc, 0x893afde0: ±5 (0x893afc30)
    Vc2OscBounds walk;          // 0x893affd4, 0x893affd8: ±5 (0x893afdf0)
    float formantTargetLeast = 0;   // 0x893afde4: 0.5 (0x893afca0)
    float formantLeast = 0;         // 0x893affe0: 0.5 (0x893afeb0)
};

[[nodiscard]] bool vc2LoadOsc(const char* path);
bool vc2OscLoaded();
const Vc2OscTables& vc2OscTables();
