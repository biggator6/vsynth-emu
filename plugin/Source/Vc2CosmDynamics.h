#pragma once
// The VC-2's COSM Amp Simulator's, Compressor's and Limiter's converters (v237): rom.4's 0x89397eb0, type 3's (block
// code 4), 0x89398fd0, type 12's (code 15), and 0x893990d0, type 13's (code 16), each on its input's parameters with
// their Matrix sums; the Amp's Gain and the Limiter's levels through rom.4's gain table (0x89541f34), the Compressor's
// and Limiter's Output Level through the level's gain (0x89397900).  rom.2's 0x894dcd80, 0x894ddc50 and 0x894ddd20 on
// another input (XtCosm's xtAmpBlock, xtCompBlock, xtLimBlock); the blocks are the V-Synth's, so XtCosm's objects play
// them.  Bit for bit: xt_vc2cosm's second part against rom.4's own.  HARDWARE_FIRMWARE.md, "The VC-2's COSM
// converters, the first batch".  The tone generator's thread.
#include "Vc2CosmConvert.h"
#include "Vc2CosmInput.h"
#include <cstdint>

// The Amp Simulator's block: the first 0x2c bytes, the rest left as they were (the binding: code 4 reads +0x10, +0x18)
struct Vc2CosmAmpBlock {
    int32_t code;              // +0x00: 4
    uint8_t ampType;           // +0x04: Amp Type's low byte
    uint8_t unread5[3];
    float gain;                // +0x08: Gain and its sum, held to 0…127, through the gain table / 32768
    Vc2CosmMod gainMod;        // +0x0c…+0x1b: Gain Env × the first velocity factor, envelope 9 (11); Gain LFO, its sum
    float bass;                // +0x1c: Bass held to 0…127, ×4/127
    float middle;              // +0x20: Middle held to 0…127, /127
    float treble;              // +0x24: Treble and its sum, held to 0…127, ×2/127
    float level;               // +0x28: Output Level through the gain table (not held) / 32768
};
static_assert(sizeof(Vc2CosmAmpBlock) == 0x2c, "rom.4's layout, the Amp Simulator's block");

// The Compressor's block: the first 0x10 bytes
struct Vc2CosmCompBlock {
    int32_t code;              // +0x00: 15
    float attack;              // +0x04: Attack and its sum, held to 0…127, /127
    float sustain;             // +0x08: Sustain and its sum, the same
    float gain;                // +0x0c: Output Level through the level's gain: (L/100)², or 1 + 3·(L − 100)/27 above 100
};
// one-off: each block is rom.4's own layout, checked by its own size; a declaration, not a job to template
static_assert(sizeof(Vc2CosmCompBlock) == 0x10, "rom.4's layout, the Compressor's block");

// The Limiter's block: the first 0x18 bytes
struct Vc2CosmLimBlock {
    int32_t code;              // +0x00: 16
    uint8_t ratio;             // +0x04: Ratio's low byte
    uint8_t unread5[3];
    float threshold;           // +0x08: Threshold Level through the gain table (not held) / 32768
    float attack;              // +0x0c: Attack and its sum, held to 0…127, through the gain table / 32768
    float release;             // +0x10: Release and its sum, the same
    float gain;                // +0x14: Output Level through the level's gain, as the Compressor's
};
static_assert(sizeof(Vc2CosmLimBlock) == 0x18, "rom.4's layout, the Limiter's block");

void vc2AmpConvert(const Vc2CosmInput& in, Vc2CosmAmpBlock& b, Vc2CosmMatrix& matrix);    // 0x89397eb0
void vc2CompConvert(const Vc2CosmInput& in, Vc2CosmCompBlock& b, Vc2CosmMatrix& matrix);  // 0x89398fd0
void vc2LimConvert(const Vc2CosmInput& in, Vc2CosmLimBlock& b, Vc2CosmMatrix& matrix);    // 0x893990d0
