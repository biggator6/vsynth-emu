#pragma once
// The OSC voice's constants from rom.4 (v230; the targets' from v232; the players' from v238, LITE's note-on's from
// v240, the analog's from v242): its routines' pool words, the three floats its install reads, the players' installs'
// tables and the analog's, read once by loadVc2 at the start, never after.  HARDWARE_FIRMWARE.md, "The VC-2's OSC
// voice", "The VC-2's OSC voice's note-on and targets", "The VC-2's players' installs and VariPhrase's note-on", "The
// VC-2's LITE note-on", "The VC-2's analog note-on".
#include "Vc2OscAnalogPool.h"
#include "Vc2OscVibrato.h"
#include <array>

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
    Vc2OscTargetPool kind6Update;   // 0x893b0254…0x893b0268: kind 6's update's own (v321)
    Vc2OscBounds walkTarget;    // 0x893afddc, 0x893afde0: ±5 (0x893afc30)
    Vc2OscBounds walk;          // 0x893affd4, 0x893affd8: ±5 (0x893afdf0)
    float formantTargetLeast = 0;   // 0x893afde4: 0.5 (0x893afca0)
    float formantLeast = 0;         // 0x893affe0: 0.5 (0x893afeb0)
    // The players' (v238): the installs' +0xa88 by the wave's type, without the flag (+0xa40) and with it: LITE's
    // three (0x89542350), VariPhrase's five (0x89542368; Vari off's are LITE's type 0); and VariPhrase's note-on's
    std::array<float, 6> liteInstalled {};
    std::array<float, 10> variInstalled {};
    float timesTenThousand = 0;     // 0x893af53c: 10000, the block's +0x60 into the object's +0x1a8
    float rateTenths = 0;           // 0x893af7c8: 12000, a loop's rate factor (tenths of a cent) into exp2's octaves
    // LITE's note-on's (v240): its own copies of those two, Vari off's (type 0) and the two heads' (type 2); 2^32, onto
    // a word FLOAT read as negative; 88200, over the root's Hz, the heads' period; 0.5, the second window's start; the
    // filter's rate per Hz of root × pitch
    float liteTimes = 0;            // 0x893c7d78: 10000
    float liteTenths = 0;           // 0x893c7f64: 12000
    float liteHeadsTenths = 0;      // 0x893c81ec: 12000
    float liteTwo32 = 0;            // 0x893c81e4
    float litePeriod = 0;           // 0x893c81f0
    float liteHalf = 0;             // 0x893c81f4
    float liteFilter = 0;           // 0x893c81f8
    // LITE's update's and settings' own copies (v324): the filter's rate per Hz, 2^32
    float liteUpdateFilter = 0;     // 0x893c84a0
    float liteSettingsTwo32 = 0;    // 0x893c8650
    float headsSeekHalf = 0;        // 0x893c6e60 (v325): the heads' seek's, the second window at half the period
    // Vari off's renders' (v261): a sample's scale, the folded way's, the gain's glide, the fade's step at the last sample
    float variOffScale = 0;         // 0x893c6420: 65536
    float variOffFold = 0;          // 0x893c6764: 45875, 70% of it
    float variOffGlide = 0;         // 0x893c6424: 1/32
    float variOffFade = 0;          // 0x893c6428
    Vc2OscAnalogPool analog;        // the analog note-on's (v242, Vc2OscAnalogPool.h)
    Vc2OscVibratoPool vibrato;      // the vibrato's (v267, Vc2OscVibrato.h)
};

[[nodiscard]] bool vc2LoadOsc(const char* path);
bool vc2OscLoaded();
const Vc2OscTables& vc2OscTables();
