#pragma once
// The VOCODER OSC's carrier (v221): voice kind 5's render state, at the voice + 0xa90 in rom.4's tone generator.  A
// naive saw through two two-state stages, a noise from a linear congruential generator through a one-pole and a
// high-pass stage, mixed by their gains and a gliding output gain; with Growl above 0 the saw is half phase- and half
// amplitude-modulated by a subharmonic near 50 Hz.  The init 0x893ebed0 (the phase started from the OS clock by
// 0x893f31a0), the settings 0x893ec450, the update 0x893ec4d0 (the balance's gains, the noise filters by pitch, the
// Dynamics on the input stage's peak) and the render 0x893ebfa0.  Bit for bit: xt_vc2carrier against rom.4's routines
// on a carrier in Sh4Interp, every word after each.  HARDWARE_FIRMWARE.md, "The VOCODER OSC's carrier";
// research/VC2_VOCAL_DESIGNER.md, "The carrier: the VOCODER OSC", "The growl, run".  Audio thread.
#include "Vc2CarrierTables.h"
#include <cstdint>

// What the carrier reads of P, the voice + 0x104, as kind 5's update leaves it (its converter's, v222)
struct Vc2CarrierParams {
    float gain;            // +0x00: the output's target
    float pitch;           // +0x04: the pitch as a ratio to 440 Hz
    float notRead;         // +0x08: the carrier reads nothing here
    float sawCutoff;       // +0x0c: Saw Wave LPF, v/127
    float sawLevel;        // +0x10: Saw Wave Level
    float noiseLowpass;    // +0x14: Noise LPF
    float noiseHighpass;   // +0x18: Noise HPF
    float noiseLevel;      // +0x1c: Noise Level
    float growl;           // +0x20: Growl Level with its keyfollow and modulation, 0…1
    float threshold;       // +0x24: Dynamics Threshold
    float sensitivity;     // +0x28: Dynamics Sensitivity
    float balance;         // +0x2c: Saw-Noise Mix Balance with its modulation, 0…2
};
static_assert(sizeof(Vc2CarrierParams) == 0x30, "rom.4's P, +0x00…+0x30 of what the carrier reads");

// A two-state stage, s1 += g·s2; s2 += g·(x − s1 − s2), as rom.4 keeps it: s2 first
struct Vc2CarrierStage {
    float s2, s1;
};

// The carrier as rom.4 lays it out, word for word
struct Vc2CarrierWords {
    float gainTarget, gain;              // +0x00, +0x04: the output's gain, gliding to its target
    float stepTarget, step;              // +0x08, +0x0c: the saw's step (the pitch), gliding halfway each sample
    float phase;                         // +0x10: the saw, in [−1, 1); [FW] single precision, as rom.4 keeps it
    float sawCoef, sawCutoff, sawGain;   // +0x14…+0x1c: the saw stages' coefficient, Saw Wave LPF, the saw's gain
    float noiseLowCoef, noiseLowpass;    // +0x20, +0x24: the noise one-pole's coefficient, Noise LPF
    float noiseHighCoef, noiseHighpass;  // +0x28, +0x2c: the noise high-pass's coefficient, Noise HPF
    float noiseGain;                     // +0x30: the noise's gain
    float threshold, sensitivity;        // +0x34, +0x38: the Dynamics'
    float growl, growlPhase, growlStep;  // +0x3c…+0x44: the growl's amount, and its oscillator, a phase in [−1, 1)
    float noiseLow;                      // +0x48: the noise's one-pole
    Vc2CarrierStage sawA, noiseHigh, sawB;   // +0x4c, +0x54, +0x5c: the saw's first stage, the noise's, the saw's second
    int32_t noiseA, noiseB;              // +0x64, +0x68: the noise generator's words
};
static_assert(sizeof(Vc2CarrierWords) == 0x6c, "rom.4's layout, the voice + 0xa90…+0xafc");

class Vc2Carrier {
public:
    static constexpr int kFrames = 32;   // a render's: two words each, the same to left and right
    // 0x893ebed0: everything from P, the phase started from `clock`, the OS's millisecond word (0x893018d0)
    void init(const Vc2CarrierParams& p, int32_t clock);
    void settings(const Vc2CarrierParams* p);   // 0x893ec450: the stored settings from P, none for no P
    // 0x893ec4d0: the targets, the growl and the gains from P (none for no P), then the coefficients; `peak` is the
    // input stage's largest peak (Vc2Unvoiced::peak, 0x899d9a5c), which the Dynamics follow
    void update(const Vc2CarrierParams* p, float peak);
    void render(float* frames);          // 0x893ebfa0: kFrames frames at the bus's scale

    Vc2CarrierWords w {};

private:
    float growlSaw(float delta, float overPi);   // the growl's branch: the saw it hands the stages, before × 0.5
};

float vc2CarrierPhaseStart(int32_t clock);   // 0x893f31a0: where a note's saw starts, by the clock
