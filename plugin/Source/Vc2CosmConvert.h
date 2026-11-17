#pragma once
// The VC-2's COSM converters' frame (v235): rom.4's dispatcher 0x89399920, which makes a COSM's block from its input
// (Vc2CosmInput.h): none when the COSM is off; else the input's two velocity factors from the note's velocity by its
// two curves and sensitivities, then the converter of its type (1…16, the Speaker Simulator's inline), none past them.
// And what the converters share as they inline it: a parameter and its Matrix sum held to 16 bits, then to 0…most; a
// depth held to ±63; a depth's square law; and (v237) the sums' calls, a keyfollow, Q, a level as 0…1 and the gain
// table.  Bit for bit: xt_vc2cosm's second part against rom.4's own, the converters it has not ported hooked.
// HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame", "The VC-2's COSM converters, the first batch".  The tone
// generator's thread.
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

// A converter's Matrix sums: each a call of the reader its COSM's byte picks (rom.4 calls it once a sum, and a call may
// refresh M), and which COSM it is, 0 or 1, for the envelopes it binds
struct Vc2CosmSums {
    const Vc2CosmInput& in;
    Vc2CosmMatrix& matrix;
    int16_t operator()(int index) const;
    int second() const;
};

// 0x89399920: the block's words from the input; the velocity factors written into the input first
void vc2CosmConvert(Vc2CosmInput& in, uint32_t* block, Vc2CosmTypes& types, const Vc2ToneShared& shared);

// A parameter plus its Matrix sum, held to 16 bits as rom.4's exts.w holds it, then to 0…most
int vc2CosmLevel(int16_t param, int16_t sum, int most);
// A depth's law: d/63 × |d/63|, `k63` the converter's own pool word
float vc2CosmDepth(int16_t depth, float k63);
// A depth plus its Matrix sum, held to ±63 (on 32 bits: no exts.w before it), through the depth's law
float vc2CosmSummedDepth(int16_t depth, int16_t sum, float k63);
// A held level 0…127 as 0…1: x/127 + 0, `x` the level as the converter has it (times a factor or not)
float vc2CosmUnit(float x);
// A cutoff and its keyfollow (v237: the TVF's, the Dynamic TVF's and the Dual Filter's 0x893976f0): (key − 60) ×
// follow × 10 (each 16 bits) ÷ 100 ÷ 127 plus the held level ÷ 127, plus 0, held to 0…1; `percent` and `k127` the
// converter's own pool words
float vc2CosmKeyed(int level, uint8_t key, int16_t follow, float percent, float k127);
// A held Q 0…127 as −1…: Q × scale ÷ 127 − 1 (the TVF's, the Dynamic TVF's and the Dual Filter's)
float vc2CosmQ(int q, float scale);
// A gain from rom.4's table (0x89541f34): its word at `index` ÷ `scale`, the converter's own pool word.  rom.4 reads
// whatever follows the table past its 128 words; no stored setting reaches there, and the port gives 0
float vc2CosmGain(int index, float scale);

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
