#pragma once
// The OSC voice's constants from rom.4 (v230): its routines' pool words and the three floats its install reads, read
// once by loadVc2 at the start (Vc2PoolTables), never after.  HARDWARE_FIRMWARE.md, "The VC-2's OSC voice".

struct Vc2OscTables {
    float waitLevel = 0;      // 0x893ae3d0: the update waits while the object's level (+0x84) is above it
    float depthScale = 0;     // 0x893ae3f4: 0.2, the block's +0x1c onto the voice's +0x70 at a note-on
    float plainHz = 0;        // 0x893ae48c: 440, a note-on's +0xa80 with no sample
    float rateTop = 0;        // 0x893ae628: 44000, over the sample's rate word
    float sampleHz = 0;       // 0x893ae630: 440, a sample's +0xa80 when it isn't pitched from its root
    float cent = 0;           // 0x893ae634: 0.01
    float exponentBase = 0;   // 0x893ae638: −5.75
    float keyCents = 0;       // 0x893ae63c: 100
    float perOctave = 0;      // 0x893ae640: 1/1200
    float levelScale = 0;     // 0x893ae644: the map's level word into exp2's exponent
    float settingsScale = 0;  // 0x893ae87c: 0.2, the settings' +0x1c onto +0x70
    float most = 0;           // 0x893aed7c: 6000, the pitch getter's bound
    float least = 0;          // 0x893aed80: −6000
    float installed4 = 0;     // 0x89542390: the install's +0xa88 for kinds 4 and 7
    float installed5 = 0;     // 0x895422f0: kind 5's
    float installed6 = 0;     // 0x895422f4: kind 6's
};

[[nodiscard]] bool vc2LoadOsc(const char* path);
bool vc2OscLoaded();
const Vc2OscTables& vc2OscTables();
