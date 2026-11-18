// rom.4's COSM blocks read into XtCosm's: see Vc2CosmAdapterBlocks.h (v262).  Every offset the block's.
#include "Vc2CosmAdapterBlocks.h"
#include "Vc2CosmAdapterWords.h"

using namespace Vc2CosmAdapter;

namespace {
constexpr int32_t kDs = 2, kDtvf = 14;   // [FW] the block codes a reader tells apart

// [FW] a setting in a block: its value, its Env depth a word on and its LFO depth three (Vc2CosmMod's), or four (the
// Dual Filter's, its Q between), which its XtCosm block names
template <class X>
struct Vc2CosmValueFields {
    float X::*value;
    float X::*env;
    float X::*lfo;
    int lfoAt = 3;
};

template <class X>
void valueAt(X& x, const Vc2CosmBlockWords& b, int at, const Vc2CosmValueFields<X>& f) {
    x.*f.value = blockFloat(b, at);
    x.*f.env = blockFloat(b, at + 1);
    x.*f.lfo = blockFloat(b, at + f.lfoAt);
}

// [FW] a block's two settings, at words `at` and `secondAt`, into its XtCosm block
template <class X>
X settingsAt(const Vc2CosmBlockWords& b, int at, int secondAt, const Vc2CosmValueFields<X>& first,
             const Vc2CosmValueFields<X>& second) {
    X x;
    valueAt(x, b, at, first);
    valueAt(x, b, secondAt, second);
    return x;
}

// [FW] an XtCosm block with its Type, the block's byte +4
template <class X>
X withType(X x, const Vc2CosmBlockWords& b) {
    x.type = static_cast<uint8_t>(b.words[0]);
    return x;
}
} // namespace

// [FW] OD/Dist's block (codes 1, 2; Vc2CosmOdBlock): Drive +4 and Tone +0x18 with their depths, Output Level +0x2c;
// DS by the code's byte
XtOdBlock Vc2CosmAdapter::odBlockOf(const Vc2CosmBlockWords& b) {
    XtOdBlock x = settingsAt<XtOdBlock>(b, 0, 5, { &XtOdBlock::drive, &XtOdBlock::driveEnv, &XtOdBlock::driveLfo },
                                        { &XtOdBlock::tone, &XtOdBlock::toneEnv, &XtOdBlock::toneLfo });
    x.level = blockFloat(b, 10);
    x.ds = static_cast<int8_t>(b.code) == kDs;
    return x;
}

// [FW] the Wave Shaper's block (Vc2CosmWsBlock): its Type +4, a byte, Drive +8 and Tone +0x1c with their depths,
// Output Level +0x30
XtWsBlock Vc2CosmAdapter::wsBlockOf(const Vc2CosmBlockWords& b) {
    XtWsBlock x = withType(settingsAt<XtWsBlock>(b, 1, 6, { &XtWsBlock::drive, &XtWsBlock::driveEnv,
                                                            &XtWsBlock::driveLfo },
                                                 { &XtWsBlock::tone, &XtWsBlock::toneEnv, &XtWsBlock::toneLfo }), b);
    x.level = blockFloat(b, 11);
    return x;
}

// [FW] the Amp Simulator's block (Vc2CosmAmpBlock): its Amp Type +4, a byte, Gain +8 with its depths, Bass +0x1c,
// Middle +0x20, Treble +0x24, Output Level +0x28
XtAmpBlock Vc2CosmAdapter::ampBlockOf(const Vc2CosmBlockWords& b) {
    XtAmpBlock x = withType(XtAmpBlock {}, b);
    valueAt(x, b, 1, { &XtAmpBlock::gain, &XtAmpBlock::gainEnv, &XtAmpBlock::gainLfo });
    x.bass = blockFloat(b, 6);
    x.middle = blockFloat(b, 7);
    x.treble = blockFloat(b, 8);
    x.level = blockFloat(b, 9);
    return x;
}

// [FW] the Resonator's Type +4, a byte, beside Size +8 and Balance +0x1c with their depths
XtResBlock Vc2CosmAdapter::resBlockOf(const Vc2CosmBlockWords& b) {
    return withType(settingsAt<XtResBlock>(b, 1, 6, { &XtResBlock::size, &XtResBlock::sizeEnv,
                                                      &XtResBlock::sizeLfo },
                                           { &XtResBlock::balance, &XtResBlock::balanceEnv,
                                             &XtResBlock::balanceLfo }), b);
}

// [FW] Lo-Fi's Bit Down +4 and Rate Down +5, signed bytes, beside Filter +8 and Balance +0x1c with their depths
XtLofiBlock Vc2CosmAdapter::lofiBlockOf(const Vc2CosmBlockWords& b) {
    XtLofiBlock x = settingsAt<XtLofiBlock>(b, 1, 6, { &XtLofiBlock::filter, &XtLofiBlock::filterEnv,
                                                       &XtLofiBlock::filterLfo },
                                            { &XtLofiBlock::balance, &XtLofiBlock::balanceEnv,
                                              &XtLofiBlock::balanceLfo });
    x.bitDown = static_cast<int8_t>(b.words[0]);
    x.rateDown = static_cast<int8_t>(b.words[0] >> 8);
    return x;
}

// [FW] the TVF's and the Dynamic TVF's block (Vc2CosmTvfBlock, v255): Q +4 and Cutoff +0x1c with their depths, the
// Own depths +0x18 and +0x30, the Filter Type +0x34; the Dynamic TVF's by the code's byte
XtTvfBlock Vc2CosmAdapter::tvfBlockOf(const Vc2CosmBlockWords& b) {
    XtTvfBlock x = settingsAt<XtTvfBlock>(b, 0, 6, { &XtTvfBlock::q, &XtTvfBlock::qEnv, &XtTvfBlock::qLfo },
                                          { &XtTvfBlock::cutoff, &XtTvfBlock::cutoffEnv, &XtTvfBlock::cutoffLfo });
    x.qOwn = blockFloat(b, 5);
    x.cutoffOwn = blockFloat(b, 11);
    x.mode = static_cast<int32_t>(b.words[12]);
    x.dynamic = static_cast<int8_t>(b.code) == kDtvf;
    return x;
}

// [FW] the Dual Filter's block (Vc2CosmDualBlock, v255): its Filter Type +4, each filter's Frequency with its Env
// depth, Q and LFO depth from +8 and +0x20
XtDualBlock Vc2CosmAdapter::dualBlockOf(const Vc2CosmBlockWords& b) {
    XtDualBlock x = settingsAt<XtDualBlock>(b, 1, 7, { &XtDualBlock::cut1, &XtDualBlock::env1, &XtDualBlock::lfo1, 4 },
                                            { &XtDualBlock::cut2, &XtDualBlock::env2, &XtDualBlock::lfo2, 4 });
    x.q1 = blockFloat(b, 3);
    x.q2 = blockFloat(b, 9);
    x.mode = static_cast<int32_t>(b.words[0]);
    return x;
}
