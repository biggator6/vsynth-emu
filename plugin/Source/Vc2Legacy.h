#pragma once
// The VC-2's LEGACY engine, the object (v211): rom.4's constructor 0x893f23c0, and its slots: 0 the reset (0x893f24b0),
// 1 the analysis (0x893f2570, Vc2LegacyAnalysis), 2 the mono synthesis (0x893f25a0), 4 the formant parameter
// (0x893f2f60), 5 the settings (0x893f2f90), 6 and 7 the hold and its level (the analysis's).  The synthesis runs in place
// on the output, which holds the carrier: its ten bands each the carrier's second difference through a resonator, × 0.5
// × the analysis's envelope, a second resonator and the band's gain; the carrier's level is followed; the bands' sum × 12
// with 16 × that level × the analysis's unvoiced row, through a six-section EQ whose first section tilts with the
// Formant Shift.  The stereo synthesis, slot 3 (0x893f2980, v212), is the same in another shape: both channels through
// each band at once, summed as they go, the levels by slot 7's.  Bit for bit: xt_vc2legacy against rom.4's object in
// Sh4Interp, its words after every block and event.  HARDWARE_FIRMWARE.md, "LEGACY's synthesis", "LEGACY's stereo
// synthesis"; VC2_VOCAL_DESIGNER.md, "LEGACY: a 10-band channel vocoder".  Audio thread.
#include "Vc2LegacyAnalysis.h"

// The object's words from +0x004, as rom.4 lays them out (+0x000 is its vtable)
struct Vc2LegacyWords {
    static constexpr int kRows = 32, kRowWords = 64, kBandWords = 24, kEqWords = 24;
    float rows[kRows][kRowWords];        // +0x0004: a band's 32 samples (mono), then the carrier's level, row 10
    float bands[kRows][kBandWords];      // +0x2004: a band's: 0, 1 the first resonator, 2, 3 the second (mono)
    float carrier[6];                    // +0x2c04: the carrier's last two samples, its level (mono's 0, 1, 2)
    float eqLow[kEqWords];               // +0x2c1c: the EQ's tilt (0…2) and its two one-poles (3, 4)
    float eqHigh[kEqWords];              // +0x2c7c: its three biquads (0…2, 3…5, 6…9)
    float formant;                       // +0x2cdc: p, from the Formant Shift ratio
    float glide;                         // +0x2ce0: the tilt's term, gliding to 1/(p² + 0.4966) by 1% a sample
};
static_assert(sizeof(Vc2LegacyWords) == 0x2ce0, "rom.4's layout, +0x0004…+0x2ce4");

// A biquad's coefficients as LEGACY's EQ keeps them: on x, x₋₁, y₋₁ and y₋₂, the last two subtracted
struct Vc2Biquad {
    float b0, b1, a1, a2;
};

class Vc2Legacy {
public:
    Vc2Legacy();                                // 0x893f23c0: its analysis's init, the states 0, p and its glide
    void reset();                               // slot 0: the same but p, which stays
    void analyze(const float* in) { analysis.block(in); }   // slot 1
    void mono(float* frames);                   // slot 2: 32 frames of two words, the carrier in each first, in place
    void stereo(float* frames);                 // slot 3 (v212): the same frames, a carrier in each word, in place
    void formant(float ratio);                  // slot 4: p = (ratio − 0.0156) / 1.9687
    void settings(float ratio, float dump);     // slot 5: the settings' Formant Shift ratio (+0x00) and dump time (+0x38)
    void hold(int controller) { analysis.hold(controller); }   // slot 6
    float holdLevel() const { return analysis.holdLevel(); }  // slot 7

    Vc2LegacyAnalysis analysis;
    Vc2LegacyWords s {};

private:
    void clear();                               // what the constructor and the reset both clear
    float band(int b, float u, int i);          // one sample of a carrier band, mono: its row's
    void glided(float* tilt);                   // the glide's sample, and the tilt's four coefficients from it
    // One channel's output EQ on one sample, on its states (mono's and stereo's left: eqLow[0…4], eqHigh[0…9]; stereo's
    // right: eqLow[5…9], eqHigh[12…21]) with this sample's tilt
    static float eqOn(float x, float* low, float* high, const float* tilt);
};
