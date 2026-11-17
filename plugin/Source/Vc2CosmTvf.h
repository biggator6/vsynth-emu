#pragma once
// The VC-2's COSM TVF's converter (v235): rom.4's 0x89398ae0, type 10's (block code 13), on its input's eight
// parameters (Filter Type, Q, Q LFO, Q Env, Cutoff, Cutoff Keyfollow, Cutoff LFO, Cutoff Env), four of them with their
// Matrix sums, the two Env depths with the dispatcher's velocity factors.  rom.2's converter 0x894dd870 on another
// input (XtCosm's xtTvfBlock); the block is the V-Synth's, so XtCosm's TVF plays it.  Bit for bit: xt_vc2cosm's second
// part against rom.4's own.  HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame".  The tone generator's thread.
#include "Vc2CosmConvert.h"
#include "Vc2CosmInput.h"
#include <cstdint>

// The TVF's block as its converter writes it: the first 0x38 of a COSM's 0x48 bytes, +0x18, +0x30 and the rest left as
// they were.  The envelopes' and LFOs' numbers are the tone's binding's (0x893b4660: code 13 reads +0xc, +0x24 and
// +0x14, +0x2c)
struct Vc2CosmTvfBlock {
    int32_t code;              // +0x00: 13
    float q;                   // +0x04: Q·0.984375/127 − 1, Q and its sum held to 0…127
    Vc2CosmMod qMod;           // +0x08…+0x17: Q Env × the second velocity factor, envelope 10 (COSM 2's 12); Q LFO
    uint32_t unread18;         // +0x18
    float cutoff;              // +0x1c: (key − 60)·KF·10/100/127 + Cutoff/127, held to 0…1
    Vc2CosmMod cutoffMod;      // +0x20…+0x2f: Cutoff Env × the first velocity factor, envelope 9 (11); Cutoff LFO
    uint32_t unread30;         // +0x30
    int32_t filterType;        // +0x34: Filter Type as it stands
};
static_assert(sizeof(Vc2CosmTvfBlock) == 0x38, "rom.4's layout, the TVF's block");

void vc2TvfConvert(const Vc2CosmInput& in, Vc2CosmTvfBlock& b, Vc2CosmMatrix& matrix);   // 0x89398ae0
