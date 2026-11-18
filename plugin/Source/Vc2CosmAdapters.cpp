// The COSM types XtCosm ports, adapted to rom.4's object: see Vc2CosmAdapters.h (v252).  Every address is rom.4's,
// every offset the object's or its block's.
#include "Vc2CosmAdapters.h"
#include "XtCosm.h"
#include <bit>
#include <cstddef>
#include <cstring>

namespace {
constexpr uint32_t kStartLevel = 0x58, kLfoLevel = 0x38;   // [FW] the bound objects' words the start values read
constexpr int32_t kResonator = 6, kTvf = 13;               // [FW] the block codes

float blockFloat(const Vc2CosmBlockWords& b, int k) { return std::bit_cast<float>(b.words[k]); }

// The bound levels a type's start values take: envelope A's and LFO A's, envelope B's and LFO B's
struct Vc2CosmStarts {
    float envA, lfoA, envB, lfoB;
};
Vc2CosmStarts startsOf(Vc2CosmNoteOn& n, const Vc2CosmObjectWords& o) {
    Vc2CosmBound& bound = n.bound();
    return { bound.word(o.bindings[0] + kStartLevel), bound.word(o.bindings[2] + kLfoLevel),
             bound.word(o.bindings[1] + kStartLevel), bound.word(o.bindings[3] + kLfoLevel) };
}

// The block's bytes from `from` into the object's at the same place past its code (+0x10 + from)
void copyBlock(Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, size_t from, size_t bytes) {
    std::memcpy(reinterpret_cast<uint8_t*>(&o) + offsetof(Vc2CosmObjectWords, code) + from,
                reinterpret_cast<const uint8_t*>(&b) + from, bytes);
}

// [FW] the Resonator's block: Type +4 (a byte), Size +8 with its Env and LFO depths +0xc, +0x14, Balance +0x1c with
// its depths +0x20, +0x28
// one-off: XtCosm's block read from rom.4's words, where xtResBlock makes it from the parameters
XtResBlock resBlockOf(const Vc2CosmBlockWords& b) {
    XtResBlock x;
    x.type = static_cast<uint8_t>(b.words[0]);
    x.size = blockFloat(b, 1);
    x.sizeEnv = blockFloat(b, 2);
    x.sizeLfo = blockFloat(b, 4);
    x.balance = blockFloat(b, 6);
    x.balanceEnv = blockFloat(b, 7);
    x.balanceLfo = blockFloat(b, 9);
    return x;
}

// The Resonator's start values (0x893a8830, rom.2's 0x894ec580): XtCosmRes's note-on's
void resStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    const Vc2CosmStarts s = startsOf(n, o);
    XtCosmRes res;
    res.noteOn(resBlockOf(b), s.envA, 0.0f, 0.0f, s.lfoA, s.envB, 0.0f, 0.0f, s.lfoB);
    o.values[0] = res.size();
    o.values[1] = res.balance();
}

// The Resonator's new block (0x893a9210, rom.2's 0x894ed0a0): XtCosmRes's targets; the block's Type as a byte and its
// words +8…+0x2f into the object, rom.4's own
void resNewBlock(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmRes res;
    res.setBlock(resBlockOf(b), o.held[0], o.held[2], o.held[1], o.held[3]);
    o.targets[0] = res.sizeTarget();
    o.targets[1] = res.balanceTarget();
    o.variant = static_cast<uint8_t>(b.words[0]);
    copyBlock(o, b, 8, 0x28);
}

// [FW] the TVF's block: Q +4 with its Env and LFO depths +8, +0x10, Cutoff +0x1c with its depths +0x20, +0x28
// The TVF's start values (0x893a8a50, rom.2's): xtTvfQ on envelope A and LFO A, then xtTvfCutoff on B
void tvfStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    const Vc2CosmStarts s = startsOf(n, o);
    const float q = xtTvfQ(blockFloat(b, 0), blockFloat(b, 1), s.envA, blockFloat(b, 3), s.lfoA);
    o.values[0] = q;
    o.values[1] = xtTvfCutoff(blockFloat(b, 6), blockFloat(b, 7), s.envB, blockFloat(b, 9), s.lfoB, q);
}

// The TVF's new block (0x893a9a60, rom.2's 0x894ed8f0): the same on the held levels into the targets; the block's
// words +4…+0x17, +0x1c…+0x2f and the Filter Type +0x34 into the object, rom.4's own: +0x18 and +0x30 stay
void tvfNewBlock(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    const float q = xtTvfQ(blockFloat(b, 0), blockFloat(b, 1), o.held[0], blockFloat(b, 3), o.held[2]);
    o.targets[0] = q;
    o.targets[1] = xtTvfCutoff(blockFloat(b, 6), blockFloat(b, 7), o.held[1], blockFloat(b, 9), o.held[3], q);
    copyBlock(o, b, 4, 0x14);
    copyBlock(o, b, 0x1c, 0x14);
    copyBlock(o, b, 0x34, 4);
}

constexpr Vc2CosmTypeRow kAdapterRows[] { { kResonator, resStartValues, resNewBlock, nullptr },
                                          { kTvf, tvfStartValues, tvfNewBlock, nullptr } };
} // namespace

std::span<const Vc2CosmTypeRow> vc2CosmAdapterRows() { return kAdapterRows; }
