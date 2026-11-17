#pragma once
// The VC-2's COSM Resonator's, Frequency Shifter's and Lo-Fi's converters (v239): rom.4's 0x89398080, type 5's (block
// code 6), 0x893991d0, type 14's (code 18), and 0x893993f0, type 15's (code 19): each a setting with a keyfollow and
// its depths, then a Dry/Effect Balance and its depths, one routine over a row past the setting's own law: the
// Resonator's Size 0x89397760 (logf(2) through `shared`), the Frequency Shifter's Depth by the keyed scale 0x89397800,
// Lo-Fi's Filter by its level keyfollow 0x89397870 (powf through `shared`).  rom.2's 0x894dcf00, 0x894ddde0 and
// 0x894ddf80 on another input (XtCosm's xtResBlock, xtFsBlock, xtLofiBlock); the blocks are the V-Synth's, so XtCosm's
// objects play them.  Bit for bit: xt_vc2cosm's second part against rom.4's own.  HARDWARE_FIRMWARE.md, "The VC-2's
// COSM converters on rom.4's libm".  The tone generator's thread.
#include "Vc2CosmConvert.h"
#include "Vc2CosmInput.h"
#include <cstdint>

class Vc2ToneShared;

// The words the three share: the Resonator's and Lo-Fi's from +8, the Frequency Shifter's from +4
struct Vc2CosmMixWords {
    float setting;             // the setting and its sum (the group's 1st) by the converter's own law
    Vc2CosmMod settingMod;     // its Env depth × the first velocity factor, envelope 9 (COSM 2's 11); its LFO, the 10th
    float balance;             // the Balance and its sum (the 2nd) × 100/127, held to 0…100, / 100 + 0
    Vc2CosmMod balanceMod;     // its Env × the second, envelope 10 (12); its LFO, the 11th
};

struct Vc2CosmResonatorBlock {
    int32_t code;              // +0x00: 6
    uint8_t filterType;        // +0x04: Filter Type's low byte
    uint8_t unread5[3];
    Vc2CosmMixWords mix;       // +0x08…+0x2f: Size, its depths, the Balance
};
static_assert(sizeof(Vc2CosmResonatorBlock) == 0x30, "rom.4's layout, the Resonator's block");

struct Vc2CosmShifterBlock {
    int32_t code;              // +0x00: 18
    Vc2CosmMixWords mix;       // +0x04…+0x2b: Effect Depth, its depths, the Balance
};
// one-off: each block is rom.4's own layout, checked by its own size; a declaration, not a job to template
static_assert(sizeof(Vc2CosmShifterBlock) == 0x2c, "rom.4's layout, the Frequency Shifter's block");

struct Vc2CosmLofiBlock {
    int32_t code;              // +0x00: 19
    uint8_t bitDown;           // +0x04: Sample Bit Down's low byte
    uint8_t rateDown;          // +0x05: Sample Rate Down's
    uint8_t unread6[2];
    Vc2CosmMixWords mix;       // +0x08…+0x2f: Filter, its depths, the Balance
};
// one-off: each block is rom.4's own layout, checked by its own size; a declaration, not a job to template
static_assert(sizeof(Vc2CosmLofiBlock) == 0x30, "rom.4's layout, Lo-Fi's block");

void vc2ResonatorConvert(const Vc2CosmInput& in, Vc2CosmResonatorBlock& b, Vc2CosmMatrix& matrix,
                         const Vc2ToneShared& shared);   // 0x89398080
void vc2ShifterConvert(const Vc2CosmInput& in, Vc2CosmShifterBlock& b, Vc2CosmMatrix& matrix,
                       const Vc2ToneShared& shared);     // 0x893991d0
void vc2LofiConvert(const Vc2CosmInput& in, Vc2CosmLofiBlock& b, Vc2CosmMatrix& matrix,
                    const Vc2ToneShared& shared);        // 0x893993f0
