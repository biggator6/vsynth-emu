#pragma once
// The VC-2's COSM Dual Filter's and TB Filter's converters (v237): rom.4's 0x893988a0, type 9's (block code 12), and
// 0x89399620, type 16's (code 20), each on its input's parameters with their Matrix sums and the dispatcher's velocity
// factors.  rom.2's 0x894dd690 and 0x894de110 on another input (XtCosm's xtDualBlock, xtTbBlock); the blocks are the
// V-Synth's, so XtCosm's objects play them.  Bit for bit: xt_vc2cosm's second part against rom.4's own.
// HARDWARE_FIRMWARE.md, "The VC-2's COSM converters, the first batch".  The tone generator's thread.
#include "Vc2CosmConvert.h"
#include "Vc2CosmInput.h"
#include <cstdint>

// A Dual Filter's filter in its block, the binding's words between the rest: its envelope at +0xc, its LFO at +0x14
struct Vc2CosmDualSide {
    float frequency;           // +0x00: Frequency, its sum and keyfollow as the TVF's Cutoff (0x893976f0)
    float env;                 // +0x04: Env depth × its velocity factor
    float q;                   // +0x08: Q·0.984375/127 − 1, Q held to 0…127 (no sum)
    int32_t envelope;          // +0x0c: Filter 1's 9 (COSM 2's 11), Filter 2's 10 (12)
    float lfo;                 // +0x10: LFO depth and its sum
    int32_t lfoNumber;         // +0x14: 0
};

struct Vc2CosmDualBlock {
    int32_t code;              // +0x00: 12
    int32_t filterType;        // +0x04: Filter Type as it stands
    Vc2CosmDualSide filter[2]; // +0x08…+0x37
};
static_assert(sizeof(Vc2CosmDualBlock) == 0x38, "rom.4's layout, the Dual Filter's block");

// The TB Filter's block: the first 0x41 bytes, the rest left as they were (the binding: code 20 reads +0x24, +0x34 and
// +0x2c, +0x3c)
struct Vc2CosmTbBlock {
    int32_t code;              // +0x00: 20
    float gain;                // +0x04: Gain's table word / 32768 × 2 (not held)
    float lpf;                 // +0x08: LPF Cutoff and its sum, held to 0…127, /127
    float lpfFollow;           // +0x0c: (KF·10 held to ±200, + 200)·4/400 − 2
    float lpfQ;                // +0x10: LPF Q and its sum, held to 0…127, /127
    float qFollow;             // +0x14: (Q Follow held to ±63, + 63)·2/126 − 1
    float hpf;                 // +0x18: HPF Cutoff held to 0…127 (no sum), /127
    float hpfFollow;           // +0x1c: as the LPF's
    Vc2CosmMod lpfMod;         // +0x20…+0x2f: LPF Env × the first velocity factor, envelope 9 (11); LPF LFO, its sum
    Vc2CosmMod hpfMod;         // +0x30…+0x3f: HPF Env × the second, envelope 10 (12); HPF LFO, no sum, no hold
    uint8_t key;               // +0x40: the note's key
    uint8_t unread41[3];
};
static_assert(sizeof(Vc2CosmTbBlock) == 0x44, "rom.4's layout, the TB Filter's block");

void vc2DualConvert(const Vc2CosmInput& in, Vc2CosmDualBlock& b, Vc2CosmMatrix& matrix);   // 0x893988a0
void vc2TbConvert(const Vc2CosmInput& in, Vc2CosmTbBlock& b, Vc2CosmMatrix& matrix);       // 0x89399620
