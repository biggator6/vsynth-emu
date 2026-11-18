// The Compressor's, Limiter's and Frequency Shifter's adapters: see Vc2CosmBufferedAdapters.h (v258).  Every address
// is rom.4's, every offset the object's or its block's.
#include "Vc2CosmBufferedAdapters.h"
#include "Vc2CosmAdapterWords.h"
#include <array>
#include <cstring>
#include <utility>

using namespace Vc2CosmAdapter;

namespace {
// [FW] the Compressor's words: the attack factor, Sustain and the gain the block gives (+0x14…), their slews from the
// note-on (D+0x8c…), and each channel's gain target, gain and envelope (D+0x98…, R's D+0xcc…)
constexpr size_t kCompWords = 0x14, kCompSlews = 0xe4, kCompLeft = 0xf0, kCompRight = 0x124;
// [FW] the Limiter's: the threshold, Attack's and Release's rates (+0x18…), the gain (+0x24), the slope (D+0x8c), the
// slewed gain (D+0xa0), the slewed three (D+0xcc…), and the two channels' gains and smoothed gains (D+0x90…)
constexpr size_t kLimRates = 0x18, kLimGain = 0x24, kLimSlope = 0xe4, kLimSlewGain = 0xf8, kLimSlews = 0x124;
constexpr size_t kLimGains = 0xe8;
constexpr size_t kFsPhase = 0xec;    // [FW] the Frequency Shifter's phase, D+0x94, which its note-on case clears
constexpr size_t kFsBytes = 0x28;    // [FW] the block's +4…+0x2b its new block copies

template <class X, size_t N>
using Vc2CosmBlockFloats = std::array<std::pair<int, float X::*>, N>;

// The block's words an XtCosm block names, by word
template <class X, size_t N>
X floatsOf(const Vc2CosmBlockWords& b, const Vc2CosmBlockFloats<X, N>& fields) {
    X x;
    for (const auto& [k, field] : fields) x.*field = blockFloat(b, k);
    return x;
}

// `n` floats into the object's words from `at`
void floatsInto(Vc2CosmObjectWords& o, size_t at, const float* v, size_t n) {
    std::memcpy(reinterpret_cast<uint8_t*>(&o) + at, v, n * sizeof(float));
}

// [FW] the Compressor's block (Vc2CosmCompBlock): Attack +4, Sustain +8, the gain +0xc
constexpr Vc2CosmBlockFloats<XtCompBlock, 3> kCompFloats { { { 0, &XtCompBlock::attack }, { 1, &XtCompBlock::sustain },
                                                             { 2, &XtCompBlock::gain } } };
// [FW] the Limiter's (Vc2CosmLimBlock): the Ratio's byte +4, Threshold +8, Attack +0xc, Release +0x10, the gain +0x14
constexpr Vc2CosmBlockFloats<XtLimBlock, 4> kLimFloats { { { 1, &XtLimBlock::threshold }, { 2, &XtLimBlock::attack },
                                                           { 3, &XtLimBlock::release }, { 4, &XtLimBlock::gain } } };
// [FW] the Frequency Shifter's (Vc2CosmShifterBlock): Depth +4, its Env and LFO depths +8, +0x10, Balance +0x18, its
// depths +0x1c, +0x24
constexpr Vc2CosmBlockFloats<XtFsBlock, 6> kFsFloats { { { 0, &XtFsBlock::depth }, { 1, &XtFsBlock::depthEnv },
                                                         { 3, &XtFsBlock::depthLfo }, { 5, &XtFsBlock::balance },
                                                         { 6, &XtFsBlock::balanceEnv },
                                                         { 8, &XtFsBlock::balanceLfo } } };

XtLimBlock limBlockOf(const Vc2CosmBlockWords& b) {
    XtLimBlock x = floatsOf(b, kLimFloats);
    x.ratio = static_cast<int8_t>(b.words[0]);
    return x;
}
} // namespace

// The new block: XtCosmComp's factor from Attack, and Sustain and the gain as they stand, into the block's places
void vc2CosmCompNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmComp& comp = n.scratch().comp;
    comp.noteOn(floatsOf(b, kCompFloats));
    floatsInto(o, kCompWords, comp.slews(), 3);
}

// The note-on case: XtCosmComp's note-on: the slews at the block's, each channel's gain and its target by Sustain and
// the factor, the envelope at the factor
void vc2CosmCompNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmComp& comp = n.scratch().comp;
    comp.noteOn(floatsOf(b, kCompFloats));
    floatsInto(o, kCompSlews, comp.slews(), 3);
    for (int ch = 0; ch < 2; ++ch) {
        const std::array<float, 3> side { comp.gainTarget(ch), comp.gain(ch), comp.envelope(ch) };
        floatsInto(o, ch == 0 ? kCompLeft : kCompRight, side.data(), side.size());
    }
}

// The new block: the Ratio's byte into +0x14, XtCosmLim's threshold, rates, slope and gain into their places
void vc2CosmLimNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmLim& lim = n.scratch().lim;
    lim.setBlock(limBlockOf(b));
    const float slope = lim.slope();
    o.variant = static_cast<uint8_t>(b.words[0]);
    floatsInto(o, kLimRates, lim.slews(), 3);
    floatsInto(o, kLimGain, lim.slews() + 3, 1);
    floatsInto(o, kLimSlope, &slope, 1);
    floatsInto(o, kLimSlewGain, lim.slews() + 3, 1);
    floatsInto(o, kLimSlews, lim.slews(), 3);
}

// The note-on case: XtCosmLim's note-on starts both channels' gains and smoothed gains at 1
void vc2CosmLimNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmLim& lim = n.scratch().lim;
    lim.noteOn(limBlockOf(b));
    const std::array<float, 4> gains { lim.gain(0), lim.smooth(0), lim.gain(1), lim.smooth(1) };
    floatsInto(o, kLimGains, gains.data(), gains.size());
}

// The start values: XtCosmFs's note-on on the bound start levels, Depth and Balance into the object's two values
void vc2CosmFsStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmFs& fs = n.scratch().fs;
    const Vc2CosmStarts s = startsOf(n, o);
    fs.noteOn(floatsOf(b, kFsFloats), s.envA, s.envA, 0.0f, s.lfoA, s.envB, s.envB, 0.0f, s.lfoB);
    o.values[0] = fs.depth();
    o.values[1] = fs.balance();
}

// The new block: XtCosmFs's targets on the held levels, and the block's words into the object's, rom.4's own
void vc2CosmFsNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmFs& fs = n.scratch().fs;
    const Vc2CosmStarts h = heldOf(o);
    fs.setBlock(floatsOf(b, kFsFloats), h.envA, h.lfoA, h.envB, h.lfoB);
    o.targets[0] = fs.depthTarget();
    o.targets[1] = fs.balanceTarget();
    copyBlock(o, b, 4, kFsBytes);
}

// The note-on case: the phase cleared, rom.4's own
void vc2CosmFsNoteOnCase(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords&) {
    const float cleared = 0.0f;
    floatsInto(o, kFsPhase, &cleared, 1);
}
