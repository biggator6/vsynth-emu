#pragma once
// The VC-2's COSM Side Band Filters' converters (v241), the last of COSM's: rom.4's 0x89398260, type 6's (block code
// 7), and 0x893984e0, type 7's (code 8).  Both make the same Detune and Width words by one routine over a row: Width
// by the keyed scale (0x89397650, vc2CosmScaled's law on its own words), Detune with its sum × 200/127 held to ±100,
// each with its Env, LFO and Own Env depths; and the LPF by its law 0x893975a0 (powf(x, 2) and logf(2) through
// `shared`).  The first has an HPF and Sub Detune, the second a Q.  rom.2's 0x894dd0a0 and 0x894dd2c0 on another input
// (XtCosm's xtSbfBlock); the blocks are the V-Synth's, so XtCosm's objects play them.  Bit for bit: xt_vc2cosm's
// second part against rom.4's own.  HARDWARE_FIRMWARE.md, "The VC-2's COSM Side Band Filters".  The tone generator's
// thread.
#include "Vc2CosmConvert.h"
#include "Vc2CosmInput.h"
#include <cstdint>

class Vc2ToneShared;

// A Side Band Filter's setting: its value, its four modulation words (the binding's) and its Own Env depth
struct Vc2CosmSbfSetting {
    float value;
    Vc2CosmMod mod;
    float own;                 // the Own Env depth by the depth's law (no velocity, no sum)
};

// The words both share, from the first's +8 and the second's +0xc
struct Vc2CosmSbfBody {
    // Detune and its sum (the group's 2nd) × 200/127, held to ±100, as it stands; Env × the second velocity factor,
    // envelope 10 (COSM 2's 12); LFO with the 11th
    Vc2CosmSbfSetting detune;
    // Width and its sum (the 1st) by the keyed scale with Width Keyfollow; Env × the first, envelope 9 (11); LFO, the
    // 10th
    Vc2CosmSbfSetting width;
};

struct Vc2CosmSbf1Block {
    int32_t code;              // +0x00: 7
    uint8_t octave;            // +0x04: Octave Select's low byte
    uint8_t unread5[3];
    Vc2CosmSbfBody body;       // +0x08…+0x37
    float hpf;                 // +0x38: −HPF ÷ 127 + 0, HPF held to 0…127 (no sum)
    float lpf;                 // +0x3c: the LPF law on LPF Cutoff (no sum) and its keyfollow
    float subDetune;           // +0x40: Sub Delay Line Detune held to ±100, as it stands
    float one;                 // +0x44: 1
};
// one-off: each block is rom.4's own layout, checked by its own size; a declaration, not a job to template
static_assert(sizeof(Vc2CosmSbf1Block) == 0x48, "rom.4's layout, the first Side Band Filter's block");

struct Vc2CosmSbf2Block {
    int32_t code;              // +0x00: 8
    uint8_t octave;            // +0x04: Octave Select's low byte
    uint8_t unread5[3];
    float q;                   // +0x08: Q·0.96875 ÷ 127 − 1, Q held to 0…127 (no sum)
    Vc2CosmSbfBody body;       // +0x0c…+0x3b
    float lpf;                 // +0x3c: as the first's
    float one;                 // +0x40: 1
};
// one-off: each block is rom.4's own layout, checked by its own size; a declaration, not a job to template
static_assert(sizeof(Vc2CosmSbf2Block) == 0x44, "rom.4's layout, the second Side Band Filter's block");

void vc2Sbf1Convert(const Vc2CosmInput& in, Vc2CosmSbf1Block& b, Vc2CosmMatrix& matrix,
                    const Vc2ToneShared& shared);   // 0x89398260
void vc2Sbf2Convert(const Vc2CosmInput& in, Vc2CosmSbf2Block& b, Vc2CosmMatrix& matrix,
                    const Vc2ToneShared& shared);   // 0x893984e0
