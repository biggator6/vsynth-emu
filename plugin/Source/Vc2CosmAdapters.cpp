// The COSM types XtCosm ports, adapted to rom.4's object: see Vc2CosmAdapters.h (v252).  Every address is rom.4's,
// every offset the object's or its block's.
#include "Vc2CosmAdapters.h"
#include "XtCosm.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {
constexpr uint32_t kStartLevel = 0x58, kLfoLevel = 0x38;   // [FW] the bound objects' words the start values read
constexpr int32_t kOd = 1, kDs = 2, kWs = 3, kAmp = 4, kResonator = 6, kTvf = 13, kLofi = 19;   // [FW] block codes
// [FW] Lo-Fi's words past the block's (v253): the Rate's mask (D+1, a byte), 1/2^Rate, 2^Bit, 2^−Bit (D+0x94…+0x9c)
constexpr size_t kLofiMask = 0x59, kLofiCoef = 0xec, kLofiUp = 0xf0, kLofiDown = 0xf4;
constexpr size_t kWsLevel = 0x40;   // [FW] the Wave Shaper's Level/8, past its block's words (v254)
constexpr size_t kGain = 0xe4;      // [FW] D+0x8c: the gain, or the Amp's first slew, which a note-on case sets

float blockFloat(const Vc2CosmBlockWords& b, int k) { return std::bit_cast<float>(b.words[k]); }

// The levels a type's laws take: envelope A's and LFO A's, envelope B's and LFO B's (the bound start levels for its
// start values, the held levels for its new block)
struct Vc2CosmStarts {
    float envA, lfoA, envB, lfoB;
};
Vc2CosmStarts startsOf(Vc2CosmNoteOn& n, const Vc2CosmObjectWords& o) {
    Vc2CosmBound& bound = n.bound();
    return { bound.word(o.bindings[0] + kStartLevel), bound.word(o.bindings[2] + kLfoLevel),
             bound.word(o.bindings[1] + kStartLevel), bound.word(o.bindings[3] + kLfoLevel) };
}
Vc2CosmStarts heldOf(const Vc2CosmObjectWords& o) { return { o.held[0], o.held[2], o.held[1], o.held[3] }; }

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

// [FW] a setting in a block: its value, its Env depth a word on and its LFO depth three (Vc2CosmMod's), which its
// XtCosm block names
template <class X>
struct Vc2CosmValueFields {
    float X::*value;
    float X::*env;
    float X::*lfo;
};

template <class X>
void valueAt(X& x, const Vc2CosmBlockWords& b, int at, const Vc2CosmValueFields<X>& f) {
    x.*f.value = blockFloat(b, at);
    x.*f.env = blockFloat(b, at + 1);
    x.*f.lfo = blockFloat(b, at + 3);
}

// [FW] a block's two settings, the first at word `at` and the second five words on, into its XtCosm block
template <class X>
X settingsAt(const Vc2CosmBlockWords& b, int at, const Vc2CosmValueFields<X>& first,
             const Vc2CosmValueFields<X>& second) {
    X x;
    valueAt(x, b, at, first);
    valueAt(x, b, at + 5, second);
    return x;
}

// [FW] an XtCosm block with its Type, the block's byte +4
template <class X>
X withType(X x, const Vc2CosmBlockWords& b) {
    x.type = static_cast<uint8_t>(b.words[0]);
    return x;
}

// XtCosm's note-on with the same levels for the start and the output, so its values are the start values' and its
// targets the new block's; a type of one value (XtCosmAmp) takes envelope A's and LFO A's alone
template <class C, class B>
void xtCosmNoteOn(C& cosm, const B& block, const Vc2CosmStarts& s) {
    if constexpr (requires { cosm.noteOn(block, 0.0f, 0.0f, 0.0f, 0.0f); })
        cosm.noteOn(block, s.envA, s.envA, 0.0f, s.lfoA);
    else
        cosm.noteOn(block, s.envA, s.envA, 0.0f, s.lfoA, s.envB, s.envB, 0.0f, s.lfoB);
}

template <class C>
using Vc2CosmGetters = std::array<float (C::*)() const, 2>;

// The getters' words into the object's two, a getter none for a type of one value
template <class C>
void gettersInto(float (&into)[2], const Vc2CosmGetters<C>& getters, const C& cosm) {
    for (size_t k = 0; k < getters.size(); ++k)
        if (getters[k] != nullptr) into[k] = (cosm.*getters[k])();
}

// A type whose XtCosm class forms one or two values (D+0xbc, +0xc0) and their targets (D+0xa4, +0xa8): its block from
// rom.4's words, its getters, and what its new block writes besides the targets
template <class C, class Block>
struct Vc2CosmValueType {
    using Cosm = C;
    Block (*blockOf)(const Vc2CosmBlockWords&);
    Vc2CosmGetters<C> values;
    Vc2CosmGetters<C> targets;
    void (*rest)(const C&, Vc2CosmObjectWords&, const Vc2CosmBlockWords&);
};

// The type's start values: its XtCosm class's note-on on the bound start levels, its values into the object's
template <const auto& T>
void valueStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    typename std::remove_cvref_t<decltype(T)>::Cosm cosm;
    xtCosmNoteOn(cosm, T.blockOf(b), startsOf(n, o));
    gettersInto(o.values, T.values, cosm);
}

// The type's new block: its XtCosm class's note-on on the held levels (which takes the block as the note's, where
// XtCosmOd's setBlock refuses another code's), its targets into the object's, then the rest
template <const auto& T>
void valueNewBlock(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    typename std::remove_cvref_t<decltype(T)>::Cosm cosm;
    xtCosmNoteOn(cosm, T.blockOf(b), heldOf(o));
    gettersInto(o.targets, T.targets, cosm);
    T.rest(cosm, o, b);
}

// A type's new block besides its targets: its Type's byte into +0x14 and `Bytes` of the block's words from +8 into
// the object's, rom.4's own (the Wave Shaper's, the Amp's, the Resonator's)
template <class C, size_t Bytes>
void typeAndWords(const C&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    o.variant = static_cast<uint8_t>(b.words[0]);
    copyBlock(o, b, 8, Bytes);
}

// [FW] a type's note-on case, inline in 0x893aa0e0's switch: words of the object into D's, rom.4's own
struct Vc2CosmCaseCopy {
    size_t from, to;
};

template <const auto& Copies>
void copyCase(Vc2CosmNoteOn&, Vc2CosmObjectWords& o) {
    auto* bytes = reinterpret_cast<uint8_t*>(&o);
    for (const Vc2CosmCaseCopy& c : Copies) std::memcpy(bytes + c.to, bytes + c.from, sizeof(float));
}

// [FW] OD/Dist's block (codes 1, 2; Vc2CosmOdBlock): Drive +4 and Tone +0x18 with their depths, Output Level +0x2c;
// DS by the code's byte
XtOdBlock odBlockOf(const Vc2CosmBlockWords& b) {
    XtOdBlock x = settingsAt<XtOdBlock>(b, 0, { &XtOdBlock::drive, &XtOdBlock::driveEnv, &XtOdBlock::driveLfo },
                                        { &XtOdBlock::tone, &XtOdBlock::toneEnv, &XtOdBlock::toneLfo });
    x.level = blockFloat(b, 10);
    x.ds = static_cast<int8_t>(b.code) == kDs;
    return x;
}

// OD/Dist's new block besides (0x893a8e50, 0x893a8f40; rom.2's 0x894ecce0, 0x894ecdd0): the block's +4…+0x2f into
// the object's, rom.4's own
void odRest(const XtCosmOd&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) { copyBlock(o, b, 4, 0x2c); }

// [FW] the Wave Shaper's block (Vc2CosmWsBlock): its Type +4, a byte, Drive +8 and Tone +0x1c with their depths,
// Output Level +0x30
XtWsBlock wsBlockOf(const Vc2CosmBlockWords& b) {
    XtWsBlock x = withType(settingsAt<XtWsBlock>(b, 1, { &XtWsBlock::drive, &XtWsBlock::driveEnv,
                                                         &XtWsBlock::driveLfo },
                                                 { &XtWsBlock::tone, &XtWsBlock::toneEnv, &XtWsBlock::toneLfo }), b);
    x.level = blockFloat(b, 11);
    return x;
}

// The Wave Shaper's new block besides (0x893a9050, rom.2's 0x894ecee0): its Type and the block's +8…+0x2f, and
// XtCosmWs's Level/8, the gain its note-on takes, into +0x40
void wsRest(const XtCosmWs& ws, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    typeAndWords<XtCosmWs, 0x28>(ws, o, b);
    putWord(o, kWsLevel, ws.gain());
}

// [FW] the Amp Simulator's block (Vc2CosmAmpBlock): its Amp Type +4, a byte, Gain +8 with its depths, Bass +0x1c,
// Middle +0x20, Treble +0x24, Output Level +0x28
XtAmpBlock ampBlockOf(const Vc2CosmBlockWords& b) {
    XtAmpBlock x = withType(XtAmpBlock {}, b);
    valueAt(x, b, 1, { &XtAmpBlock::gain, &XtAmpBlock::gainEnv, &XtAmpBlock::gainLfo });
    x.bass = blockFloat(b, 6);
    x.middle = blockFloat(b, 7);
    x.treble = blockFloat(b, 8);
    x.level = blockFloat(b, 9);
    return x;
}

// [FW] the Resonator's Type +4, a byte, beside Size +8 and Balance +0x1c with their depths
XtResBlock resBlockOf(const Vc2CosmBlockWords& b) {
    return withType(settingsAt<XtResBlock>(b, 1, { &XtResBlock::size, &XtResBlock::sizeEnv, &XtResBlock::sizeLfo },
                                           { &XtResBlock::balance, &XtResBlock::balanceEnv,
                                             &XtResBlock::balanceLfo }), b);
}

// [FW] Lo-Fi's Bit Down +4 and Rate Down +5, signed bytes, beside Filter +8 and Balance +0x1c with their depths
XtLofiBlock lofiBlockOf(const Vc2CosmBlockWords& b) {
    XtLofiBlock x = settingsAt<XtLofiBlock>(b, 1, { &XtLofiBlock::filter, &XtLofiBlock::filterEnv,
                                                    &XtLofiBlock::filterLfo },
                                            { &XtLofiBlock::balance, &XtLofiBlock::balanceEnv,
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

// The types' start values (0x893a85f0's cases: OD/Dist's 0x893a8658, 0x893a86e0, the Wave Shaper's 0x893a8770, the
// Amp's 0x893a87f0, the Resonator's 0x893a8830, Lo-Fi's 0x893a8bb0) and new blocks (the Amp's 0x893a9150, the
// Resonator's 0x893a9210; rom.2's 0x894ecfe0, 0x894ed0a0)
constexpr Vc2CosmValueType<XtCosmOd, XtOdBlock> kOdType { odBlockOf, { &XtCosmOd::drive, &XtCosmOd::tone },
                                                          { &XtCosmOd::driveTarget, &XtCosmOd::toneTarget }, odRest };
constexpr Vc2CosmValueType<XtCosmWs, XtWsBlock> kWsType { wsBlockOf, { &XtCosmWs::drive, &XtCosmWs::tone },
                                                          { &XtCosmWs::driveTarget, &XtCosmWs::toneTarget }, wsRest };
constexpr Vc2CosmValueType<XtCosmAmp, XtAmpBlock> kAmpType { ampBlockOf, { &XtCosmAmp::gain, nullptr },
                                                             { &XtCosmAmp::gainTarget, nullptr },
                                                             typeAndWords<XtCosmAmp, 0x24> };
constexpr Vc2CosmValueType<XtCosmRes, XtResBlock> kResType { resBlockOf, { &XtCosmRes::size, &XtCosmRes::balance },
                                                             { &XtCosmRes::sizeTarget, &XtCosmRes::balanceTarget },
                                                             typeAndWords<XtCosmRes, 0x28> };
constexpr Vc2CosmValueType<XtCosmLofi, XtLofiBlock> kLofiType { lofiBlockOf,
                                                                { &XtCosmLofi::filter, &XtCosmLofi::balance },
                                                                { &XtCosmLofi::filterTarget,
                                                                  &XtCosmLofi::balanceTarget }, lofiRest };

// The note-on cases (v254): OD/Dist's (0x893aa2b2, 0x893aa2c0) its Output Level (+0x3c) as the gain, the Wave
// Shaper's (0x893aa2d0) its Level/8 (+0x40), the Amp's (0x893aa2e0) its Level, Treble, Middle and Bass (+0x38…+0x2c)
// as the slews D+0x8c…+0x98; XtCosm's note-ons end so (0x894ee142, 0x894ee160, 0x894ee170)
constexpr Vc2CosmCaseCopy kOdCase[] { { 0x3c, kGain } };
constexpr Vc2CosmCaseCopy kWsCase[] { { kWsLevel, kGain } };
constexpr Vc2CosmCaseCopy kAmpCase[] { { 0x38, kGain }, { 0x34, kGain + 4 }, { 0x30, kGain + 8 },
                                       { 0x2c, kGain + 12 } };

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

constexpr Vc2CosmTypeRow kAdapterRows[] {
    { kOd, valueStartValues<kOdType>, valueNewBlock<kOdType>, copyCase<kOdCase> },
    { kDs, valueStartValues<kOdType>, valueNewBlock<kOdType>, copyCase<kOdCase> },
    { kWs, valueStartValues<kWsType>, valueNewBlock<kWsType>, copyCase<kWsCase> },
    { kAmp, valueStartValues<kAmpType>, valueNewBlock<kAmpType>, copyCase<kAmpCase> },
    { kResonator, valueStartValues<kResType>, valueNewBlock<kResType>, nullptr },
    { kTvf, tvfStartValues, tvfNewBlock, nullptr },
    { kLofi, valueStartValues<kLofiType>, valueNewBlock<kLofiType>, nullptr } };
} // namespace

std::span<const Vc2CosmTypeRow> vc2CosmAdapterRows() { return kAdapterRows; }
