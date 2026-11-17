#pragma once
// The VC-2's COSM converters' frame (v235): rom.4's dispatcher 0x89399920, which makes a COSM's block from its input
// (Vc2CosmInput.h): none when the COSM is off; else the input's two velocity factors from the note's velocity by its
// two curves and sensitivities, then the converter of its type (1…16, the Speaker Simulator's inline), none past them.
// And what the converters share as they inline it: a parameter and its Matrix sum held to 16 bits, then to 0…most; a
// depth held to ±63; a depth's square law.  Bit for bit: xt_vc2cosm's second part against rom.4's own, the converters
// it has not ported hooked.  HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame".  The tone generator's thread.
#include "Vc2CosmInput.h"
#include <cstdint>

class Vc2ToneShared;

inline constexpr int kVc2CosmBlockWords = 0x48 / 4;   // [FW] a COSM's block: its code at +0, the rest by its type

// The dispatcher's converters, by the input's type (rom.4's 0x89397980…0x89399620, each one's block on `block`)
class Vc2CosmTypes {
public:
    virtual ~Vc2CosmTypes() = default;
    virtual void convert(int type, const Vc2CosmInput& in, uint32_t* block) = 0;
};

// 0x89399920: the block's words from the input; the velocity factors written into the input first
void vc2CosmConvert(Vc2CosmInput& in, uint32_t* block, Vc2CosmTypes& types, const Vc2ToneShared& shared);

// A parameter plus its Matrix sum, held to 16 bits as rom.4's exts.w holds it, then to 0…most
int vc2CosmLevel(int16_t param, int16_t sum, int most);
// A depth's law: d/63 × |d/63|, `k63` the converter's own pool word
float vc2CosmDepth(int16_t depth, float k63);
// A depth plus its Matrix sum, held to ±63 (on 32 bits: no exts.w before it), through the depth's law
float vc2CosmSummedDepth(int16_t depth, int16_t sum, float k63);

// A modulated setting's four words in a block, as the converters lay them: its envelope's depth and number, its LFO's
// depth and number, which the tone's binding (0x893b4660) reads by the block's code
struct Vc2CosmMod {
    float env;
    int32_t envelope;
    float lfo;
    int32_t lfoNumber;
};
static_assert(sizeof(Vc2CosmMod) == 0x10, "rom.4's layout, a block's modulation words");

// A setting's: its Env depth's law by a velocity factor and its envelope; its LFO depth with its sum; LFO number 0
Vc2CosmMod vc2CosmModOf(int16_t env, float velocity, int32_t envelope, int16_t lfo, int16_t lfoSum, float k63);
