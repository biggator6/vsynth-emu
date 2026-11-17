#pragma once
// The VC-2's COSM constants from rom.4 (v234): the Growl's, its DSP's and its update's pool words; the dispatcher's and
// the TVF's converter's (v235); the Dynamic TVF's, the Dual Filter's, the Amp Simulator's, the Compressor's, the
// Limiter's and the TB Filter's, and the gain table they share (v237); OD/Dist's, the Wave Shaper's, the Resonator's,
// the Frequency Shifter's and Lo-Fi's (v239); the Side Band Filters' (v241); read once by loadVc2 at the start
// (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "The VC-2's COSM type 8, the Growl", "The VC-2's COSM converters'
// frame", "The VC-2's COSM converters, the first batch", "The VC-2's COSM converters on rom.4's libm", "The VC-2's COSM
// Side Band Filters".
#include "Vc2Math.h"
#include <cstdint>

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

// The TVF's converter's (0x89398da0…0x89398db7, v235), and the Dynamic TVF's (0x89399020…0x89399037, v237)
struct Vc2CosmTvfPool {
    float percent = 0;     // 100: the keyfollow's per cent
    float k127 = 0;        // 127: the keyfollow's per step
    float k63 = 0;         // 63: a depth's per step
    float qScale = 0;      // 0.984375: Q's top below 1
};

// The Dual Filter's (0x89398b4c, 0x89398b50) and its keyfollow's, 0x893976f0's (0x89397920, 0x89397924)
struct Vc2CosmDualPool {
    float qScale = 0;      // 0.984375
    float k63 = 0;         // 63
    float percent = 0;     // 100
    float k127 = 0;        // 127
};

// The Amp Simulator's (0x893980b4, 0x893980c0) and its depths', 0x89397450's (0x893976b0)
struct Vc2CosmAmpPool {
    float bass = 0;        // 4: the Bass's top
    float gainScale = 0;   // 32768: a gain table word's 1
    float k63 = 0;         // 63
};

// The level's gain, 0x89397900's, the Compressor's and the Limiter's (0x89397920, 0x89397bc0, 0x89397bc4)
struct Vc2CosmLevelGainPool {
    float percent = 0;     // 100: (L/100)² up to it
    float over = 0;        // 27: above it, 1 + 3·(L − 100)/27
    float overGain = 0;    // 3
};

// The TB Filter's (0x8939979c…0x893997a7, 0x893999a0…0x893999af)
struct Vc2CosmTbPool {
    float lpfFollow = 0, lpfFollowFrom = 0;   // 4, −2: the LPF keyfollow's (KF·10 + 200)·4/400 − 2
    float k63 = 0;                            // 63
    float hpfFollow = 0, hpfFollowFrom = 0;   // the HPF keyfollow's, the same values
    float gainScale = 0;                      // 32768
};

// v239's.  The level keyfollow's: 0x89397480 and its copy 0x89397510 read 0x893976b4…0x893976bf, Lo-Fi's 0x89397870
// 0x8939793c, 0x89397940 and 0x89397934
struct Vc2CosmLevelKeyPool {
    float scale = 0;       // 0.9: the held level's top above the floor
    float from = 0;        // 0.1: the floor, added and held to
    float cents = 0;       // 1200: the keyfollow's per octave
};

// OD/Dist's (0x89397bcc) and the Wave Shaper's (0x89397e4c), their depths' 0x89397450 (0x893976b0), their Tone's
struct Vc2CosmDrivePool {
    float gainScale = 0;   // 32768: a gain table word's 1
    float k63 = 0;         // 63
    Vc2CosmLevelKeyPool key {};
};

// The Resonator's Size, 0x89397760 (0x89397920, 0x8939792c, 0x89397930), and its depths' (0x893976b0)
struct Vc2CosmResonatorPool {
    float percent = 0;     // 100
    float twelve = 0;      // 12
    float sizeScale = 0;   // 1.6: the keyfollow's ln 2 over it
    float k63 = 0;         // 63
};

// The Frequency Shifter's Depth, 0x89397800 (0x89397934), and its depths' (0x89399500)
struct Vc2CosmShifterPool {
    float cents = 0;       // 1200
    float k63 = 0;         // 63
};

// Lo-Fi's Filter, 0x89397870, and its depths' (0x89399500)
struct Vc2CosmLofiPool {
    Vc2CosmLevelKeyPool key {};
    float k63 = 0;         // 63
};

// v241's.  The Side Band Filters' shared helpers': the LPF's 0x893975a0 (0x893976c4, 0x893976c8, 0x893976cc,
// 0x893976d4) and the Width's 0x89397650 (0x893976bc)
struct Vc2CosmSideBandLaws {
    float half = 0;        // 0.5: the LPF's ½ × Cutoff ÷ 127 + ½, and its floor
    float percent = 0;     // 100
    float twelve = 0;      // 12
    float quarter = 0;     // 0.25: ln 2 × x² × ¼
    float widthCents = 0;  // 1200
};

// Each Side Band Filter's own: its depths' 63, Detune Own's 63 and the second's Q scale.  The first's: 0x89397450's
// (0x893976b0), Detune Own's inline (0x893985e0); the second's all inline (0x89398890), Q's 0.96875 (0x893985e4)
struct Vc2CosmSideBandPool {
    float k63 = 0;
    float ownK63 = 0;
    float qScale = 0;
};

// The COSM object's install (v249, Vc2CosmObject.cpp): the DSP routines by the code byte, mono (0x8993fb50) and stereo
// (0x8993fba4), read at the byte signed, so 128 either side, past code 20 what follows them at the start; and its floats
// from 0x895423b4, 28 for the mono types and their variants, then the stereo's 28 (0x89542424) and on as far as the
// stereo Amp's at a Type byte of 255
struct Vc2CosmObjectPool {
    static constexpr int kCodeReach = 128, kTypes = 28, kFactors = 289;
    uint32_t mono[2 * kCodeReach] {};
    uint32_t stereo[2 * kCodeReach] {};
    float factors[kFactors] {};
    // The pools' static initializer's three floats (v250, Vc2CosmPools): 0x89999290's, 0x89999654's, 0x89999658's
    float poolFloats[3] {};
};

struct Vc2CosmTables {
    static constexpr int kGains = 128;   // [FW] the gain table's words, 0…32768, before the next table's
    Vc2CosmGrowlPool growl {};
    Vc2CosmTvfPool tvf {}, dtvf {};
    Vc2CosmDualPool dual {};
    Vc2CosmAmpPool amp {};
    Vc2CosmLevelGainPool levelGain {};
    Vc2CosmTbPool tb {};
    Vc2CosmDrivePool od {}, ws {};
    Vc2CosmResonatorPool resonator {};
    Vc2CosmShifterPool shifter {};
    Vc2CosmLofiPool lofi {};
    Vc2CosmSideBandLaws sideBand {};
    Vc2CosmSideBandPool sbf1 {}, sbf2 {};
    float velocityScale = 0;   // 32768: the dispatcher's, a velocity factor's 0…32768 to 0…1 (0x893999ac, v235)
    float limiterScale = 0;    // 32768: the Limiter's, a gain table word's 1 (0x89399270)
    uint16_t gains[kGains] {};   // 0x89541f34: about 32768·(i/127)², the Amp's, Limiter's and TB Filter's (v237)
    Vc2CosmObjectPool object {};
};

[[nodiscard]] bool vc2LoadCosm(const char* path);
bool vc2CosmLoaded();
const Vc2CosmTables& vc2CosmTables();
