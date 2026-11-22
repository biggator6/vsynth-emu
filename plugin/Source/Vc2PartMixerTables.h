#pragma once
// The part mixer's constants from rom.4 (v227): its routines' pool words, read once by loadVc2 at the start
// (Vc2PoolTables), never after.  HARDWARE_FIRMWARE.md, "The part mixer".

// A routine's targets' pool: the levels' 0.025 and the EXTIN flag's 2^31 (2147483392, a step under)
struct Vc2MixerTargetPool {
    float scale = 0;
    float toBus = 0;
};

struct Vc2PartMixerTables {
    float toFloat = 0;              // 2^-31, the input frames' words (0x893b1a88)
    float toRing = 0;               // the float a unit under 2^-31, the detector's ring's (0x893b1aa8)
    float ease[2] = {};             // 0.0009375, a level's step a sample, the two paths' (0x893b1ab8, 0x893b1d44)
    float perLevel = 0;             // 1/127, a setting's (0x893b15d8)
    Vc2MixerTargetPool pool[2] {};  // the level routine's (0x893b15f0, 0x893b17d0), the targets' (0x893b2068, 0x893b2074)
    float blockSeconds = 0;         // v311: a block's seconds, 32/44100 (0x893b17ec), the update's and the schedule's
    float updateSeconds = 0;        // v311: the update's tick, 0.01 s (0x893b17f4)
};

[[nodiscard]] bool vc2LoadMixer(const char* path);
bool vc2MixerLoaded();
const Vc2PartMixerTables& vc2MixerTables();
