#pragma once
// The VC-2's COSM OD/Dist's and Wave Shaper's converters (v239): rom.4's 0x89397980, type 1's (block code 1, or 2 when
// its Type isn't 0), and 0x89397cf0, type 2's (code 3, its Type as a byte), one routine over a row: Output Level and
// Drive through the gain table, Drive's depths, Tone through the level keyfollow (0x89397480, the Wave Shaper's copy
// 0x89397510, powf through `shared`) and Tone's depths, four Matrix sums.  rom.2's 0x894dc8b0 and 0x894dcbf0 on another
// input (XtCosm's xtOdBlock, xtWsBlock); the blocks are the V-Synth's, so XtCosm's objects play them.  Bit for bit:
// xt_vc2cosm's second part against rom.4's own.  HARDWARE_FIRMWARE.md, "The VC-2's COSM converters on rom.4's libm".
// The tone generator's thread.
#include "Vc2CosmConvert.h"
#include "Vc2CosmInput.h"
#include <cstdint>

class Vc2ToneShared;

// The words both blocks share, from OD/Dist's +4 and the Wave Shaper's +8; the binding reads the envelopes' and LFOs'
// numbers by code
struct Vc2CosmDriveBody {
    float drive;               // the gain table at Drive and its sum (the group's 1st), held, / 32768
    Vc2CosmMod driveMod;       // Drive Env × the first velocity factor, envelope 9 (COSM 2's 11); Drive LFO, the 10th
    float tone;                // the level keyfollow of Tone and its sum (the 2nd) by Tone Keyfollow
    Vc2CosmMod toneMod;        // Tone Env × the second, envelope 10 (12); Tone LFO, the 11th
    float level;               // the gain table at Output Level (not held) / 32768
};

struct Vc2CosmOdBlock {
    int32_t code;              // +0x00: 1, or 2 when Type isn't 0
    Vc2CosmDriveBody body;     // +0x04…+0x2f
};
static_assert(sizeof(Vc2CosmOdBlock) == 0x30, "rom.4's layout, OD/Dist's block");

struct Vc2CosmWsBlock {
    int32_t code;              // +0x00: 3
    uint8_t type;              // +0x04: Type's low byte
    uint8_t unread5[3];
    Vc2CosmDriveBody body;     // +0x08…+0x33
};
// one-off: each block is rom.4's own layout, checked by its own size; a declaration, not a job to template
static_assert(sizeof(Vc2CosmWsBlock) == 0x34, "rom.4's layout, the Wave Shaper's block");

void vc2OdConvert(const Vc2CosmInput& in, Vc2CosmOdBlock& b, Vc2CosmMatrix& matrix,
                  const Vc2ToneShared& shared);   // 0x89397980
void vc2WsConvert(const Vc2CosmInput& in, Vc2CosmWsBlock& b, Vc2CosmMatrix& matrix,
                  const Vc2ToneShared& shared);   // 0x89397cf0
