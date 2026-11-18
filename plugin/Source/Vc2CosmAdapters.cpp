// The COSM types XtCosm ports, adapted to rom.4's object: see Vc2CosmAdapters.h (v252).  Every address is rom.4's,
// every offset the object's or its block's.
#include "Vc2CosmAdapters.h"
#include "XtCosm.h"
#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {
constexpr uint32_t kStartLevel = 0x58, kLfoLevel = 0x38;   // [FW] the bound objects' words the start values read
constexpr int32_t kResonator = 6, kTvf = 13, kLofi = 19;   // [FW] the block codes
// [FW] Lo-Fi's words past the block's (v253): the Rate's mask (D+1, a byte), 1/2^Rate, 2^Bit, 2^−Bit (D+0x94…+0x9c)
constexpr size_t kLofiMask = 0x59, kLofiCoef = 0xec, kLofiUp = 0xf0, kLofiDown = 0xf4;

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

// A value's bytes into the object at `at`, where a type keeps a word of its own
template <class V>
void putWord(Vc2CosmObjectWords& o, size_t at, V v) {
    std::memcpy(reinterpret_cast<uint8_t*>(&o) + at, &v, sizeof v);
}

// [FW] the Resonator's and Lo-Fi's blocks: two values, each with its Env and LFO depths, at +8, +0xc, +0x14 and +0x1c,
// +0x20, +0x28 (words 1, 2, 4 and 6, 7, 9), which their XtCosm blocks name
template <class X>
struct Vc2CosmPairFields {
    float X::*first;
    float X::*firstEnv;
    float X::*firstLfo;
    float X::*second;
    float X::*secondEnv;
    float X::*secondLfo;
};

template <class X>
X pairBlockOf(const Vc2CosmBlockWords& b, const Vc2CosmPairFields<X>& f) {
    X x;
    x.*f.first = blockFloat(b, 1);
    x.*f.firstEnv = blockFloat(b, 2);
    x.*f.firstLfo = blockFloat(b, 4);
    x.*f.second = blockFloat(b, 6);
    x.*f.secondEnv = blockFloat(b, 7);
    x.*f.secondLfo = blockFloat(b, 9);
    return x;
}

// A type whose XtCosm class has two values and their targets (XtCosmRes, XtCosmLofi): its block from rom.4's words,
// its getters, and what its new block writes besides the targets
template <class C, class Block>
struct Vc2CosmPairType {
    using Cosm = C;
    Block (*blockOf)(const Vc2CosmBlockWords&);
    float (C::*value0)() const;
    float (C::*value1)() const;
    float (C::*target0)() const;
    float (C::*target1)() const;
    void (*rest)(const C&, Vc2CosmObjectWords&, const Vc2CosmBlockWords&);
};

// The type's start values: its XtCosm class's note-on on the bound start levels, its two values into the object's
template <const auto& T>
void pairStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    const Vc2CosmStarts s = startsOf(n, o);
    typename std::remove_cvref_t<decltype(T)>::Cosm cosm;
    cosm.noteOn(T.blockOf(b), s.envA, 0.0f, 0.0f, s.lfoA, s.envB, 0.0f, 0.0f, s.lfoB);
    o.values[0] = (cosm.*T.value0)();
    o.values[1] = (cosm.*T.value1)();
}

// The type's new block: its XtCosm class's on the held levels, its targets into the object's, then the rest
template <const auto& T>
void pairNewBlock(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    typename std::remove_cvref_t<decltype(T)>::Cosm cosm;
    cosm.setBlock(T.blockOf(b), o.held[0], o.held[2], o.held[1], o.held[3]);
    o.targets[0] = (cosm.*T.target0)();
    o.targets[1] = (cosm.*T.target1)();
    T.rest(cosm, o, b);
}

// [FW] the Resonator's Type +4, a byte, beside Size and Balance
XtResBlock resBlockOf(const Vc2CosmBlockWords& b) {
    XtResBlock x = pairBlockOf(b, Vc2CosmPairFields<XtResBlock> { &XtResBlock::size, &XtResBlock::sizeEnv,
                                   &XtResBlock::sizeLfo, &XtResBlock::balance, &XtResBlock::balanceEnv,
                                   &XtResBlock::balanceLfo });
    x.type = static_cast<uint8_t>(b.words[0]);
    return x;
}

// The Resonator's new block besides (0x893a9210, rom.2's 0x894ed0a0): the Type's byte into +0x14, the block's
// +8…+0x2f into the object's, rom.4's own
void resRest(const XtCosmRes&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    o.variant = static_cast<uint8_t>(b.words[0]);
    copyBlock(o, b, 8, 0x28);
}

// [FW] Lo-Fi's Bit Down +4 and Rate Down +5, signed bytes, beside Filter and Balance
XtLofiBlock lofiBlockOf(const Vc2CosmBlockWords& b) {
    XtLofiBlock x = pairBlockOf(b, Vc2CosmPairFields<XtLofiBlock> { &XtLofiBlock::filter, &XtLofiBlock::filterEnv,
                                    &XtLofiBlock::filterLfo, &XtLofiBlock::balance, &XtLofiBlock::balanceEnv,
                                    &XtLofiBlock::balanceLfo });
    x.bitDown = static_cast<int8_t>(b.words[0]);
    x.rateDown = static_cast<int8_t>(b.words[0] >> 8);
    return x;
}

// Lo-Fi's new block besides (0x893a9ed0, rom.2's 0x894edd60): the two bytes into +0x14, +0x15 and the block's
// +8…+0x2f into the object's, rom.4's own; the mask and scales XtCosmLofi forms from them into its own words
void lofiRest(const XtCosmLofi& lofi, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    copyBlock(o, b, 4, 2);
    copyBlock(o, b, 8, 0x28);
    putWord(o, kLofiMask, static_cast<uint8_t>(lofi.rateMask()));
    putWord(o, kLofiCoef, lofi.rateCoef());
    putWord(o, kLofiUp, lofi.upScale());
    putWord(o, kLofiDown, lofi.downScale());
}

// The Resonator's start values 0x893a8830 (rom.2's 0x894ec580) and new block; Lo-Fi's 0x893a8bb0 (rom.2's 0x894eca70)
constexpr Vc2CosmPairType<XtCosmRes, XtResBlock> kResType { resBlockOf, &XtCosmRes::size, &XtCosmRes::balance,
                                                            &XtCosmRes::sizeTarget, &XtCosmRes::balanceTarget,
                                                            resRest };
constexpr Vc2CosmPairType<XtCosmLofi, XtLofiBlock> kLofiType { lofiBlockOf, &XtCosmLofi::filter, &XtCosmLofi::balance,
                                                               &XtCosmLofi::filterTarget, &XtCosmLofi::balanceTarget,
                                                               lofiRest };

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

constexpr Vc2CosmTypeRow kAdapterRows[] { { kResonator, pairStartValues<kResType>, pairNewBlock<kResType>, nullptr },
                                          { kTvf, tvfStartValues, tvfNewBlock, nullptr },
                                          { kLofi, pairStartValues<kLofiType>, pairNewBlock<kLofiType>, nullptr } };
} // namespace

std::span<const Vc2CosmTypeRow> vc2CosmAdapterRows() { return kAdapterRows; }
