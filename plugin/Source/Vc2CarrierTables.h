#pragma once
// The VOCODER OSC's constants from rom.4: its carrier's routines' pool words and the pitch factor its rate setter writes
// (v221), its converter's and kind 5's update's (v222); read once by loadVc2 at the start (Vc2RomTables), never after.
// HARDWARE_FIRMWARE.md, "The VOCODER OSC's carrier", "The VOCODER OSC's converter and kind 5's update".
#include <cstdint>

// The render's constants by path: the plain path's (0x893ec230…0x893ec23c) and the growl's (its own pool from
// 0x893ec4ac)
struct Vc2CarrierPool {
    float half;       // the step's glide, halfway to its target a sample (the growl's: also its saw × it)
    float minusTwo;   // a phase's wrap
    float glide;      // the gain's glide a sample, 0.00756
    float toBus;      // 2^31, the voice buffer's integer scale
};

struct Vc2CarrierTables {
    float pitchFactor = 0;   // 880/44100, what the rate setter 0x893ec6a0 writes at 0x899d9964 (0x893ec6f8)
    float growlStart = 0;    // 50: the init's growl step, × the pitch factor (0x893ec228)
    float noiseScale = 0;    // the noise's word to ±1, 2^-31 an ulp short (0x893ec22c)
    Vc2CarrierPool plain {}, growl {};
    float overPi = 0;        // 1/π, the growl's phase shift (0x893ec4a0)
    float k440 = 0, k50 = 0; // the growl's divider, the step × 440 ÷ 50 (0x893ec4b0, 0x893ec4b4)
    // The update's (0x893ec6c0…0x893ec6ec): the noise lowpass under pitch 1, × (lowBase + lowSlope × pitch); the
    // high-pass's pitch, at least minPitch, into highA p² + highB p + highC; the Dynamics' threshold floor, and their
    // half, 0.3, 0.9 and 0.1
    float lowBase = 0, lowSlope = 0, minPitch = 0, highA = 0, highB = 0, highC = 0;
    float floor = 0, dynHalf = 0, dynLow = 0, dynSawBase = 0, dynSaw = 0;
    float foldAt = 0, foldNeg = 0, three = 0, four = 0;   // the sine's and cosine's, 0x893f3210 and 0x893f3280
    float clockScale = 0, logistic = 0;   // the phase's start (0x893f31a0): the clock ÷ 32767, then × 3.997
    // The converter's own part (0x8939db00, v222): a level's 1/127, the velocity factor's 32768, the Matrix sums'
    // 12000, Growl Level's 127, its keyfollow's 1200, the balance's 64, and the depth curve's 63 (0x8939c0b0)
    float perLevel = 0, velocityScale = 0, sumScale = 0, growlScale = 0, keyfollowScale = 0, balanceScale = 0;
    float depthScale = 0;
    // Kind 5's update (0x893afff0): the pitch's cents held to ±6000, × 1/1200 into exp2, the ratio held to ±32
    float centsMax = 0, centsMin = 0, perOctave = 0, ratioMax = 0, ratioMin = 0;
};

[[nodiscard]] bool vc2LoadCarrier(const char* path);
bool vc2CarrierLoaded();
const Vc2CarrierTables& vc2CarrierTables();
