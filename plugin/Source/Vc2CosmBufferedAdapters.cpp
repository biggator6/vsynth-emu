// The Compressor's, Limiter's and Frequency Shifter's adapters: see Vc2CosmBufferedAdapters.h (v258).  Every address
// is rom.4's, every offset the object's or its block's.
#include "Vc2CosmBufferedAdapters.h"
#include "Vc2CosmAdapterBlocks.h"
#include "Vc2CosmAdapterWords.h"
#include "XtFloat.h"
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

// A new block's two targets: the XtCosm class's setBlock on the held levels, its getters into the object's
template <class C, class B>
void targetsOn(C& cosm, const B& block, Vc2CosmObjectWords& o, float (C::*first)() const, float (C::*second)() const) {
    const Vc2CosmStarts h = heldOf(o);
    cosm.setBlock(block, h.envA, h.lfoA, h.envB, h.lfoB);
    o.targets[0] = (cosm.*first)();
    o.targets[1] = (cosm.*second)();
}

constexpr int32_t kSbf2 = 8;   // [FW] the second Side Band Filter's block code
// [FW] the first's block (Vc2CosmSbf1Block): Detune +8 and Width +0x20, each with its Env, LFO and Own depths, the HPF
// +0x38, the LPF +0x3c, the Sub Detune +0x40
constexpr Vc2CosmBlockFloats<XtSbfBlock, 11> kSbf1Floats { {
    { 1, &XtSbfBlock::detune }, { 2, &XtSbfBlock::detuneEnv }, { 4, &XtSbfBlock::detuneLfo },
    { 6, &XtSbfBlock::detuneOwn }, { 7, &XtSbfBlock::width }, { 8, &XtSbfBlock::widthEnv },
    { 10, &XtSbfBlock::widthLfo }, { 12, &XtSbfBlock::widthOwn }, { 13, &XtSbfBlock::hpf }, { 14, &XtSbfBlock::lpf },
    { 15, &XtSbfBlock::subDetune } } };
// [FW] the second's (Vc2CosmSbf2Block): Q +8, Detune +0xc and Width +0x24 with their depths, the LPF +0x3c
constexpr Vc2CosmBlockFloats<XtSbfBlock, 10> kSbf2Floats { {
    { 1, &XtSbfBlock::q }, { 2, &XtSbfBlock::detune }, { 3, &XtSbfBlock::detuneEnv }, { 5, &XtSbfBlock::detuneLfo },
    { 7, &XtSbfBlock::detuneOwn }, { 8, &XtSbfBlock::width }, { 9, &XtSbfBlock::widthEnv },
    { 11, &XtSbfBlock::widthLfo }, { 13, &XtSbfBlock::widthOwn }, { 14, &XtSbfBlock::lpf } } };
// [FW] the object's words: the first's 1 − Detune/2400, HPF/2 and 1 − Sub/2400; the second's 1 − Detune/2400
constexpr size_t kSbf1Detune = 0x18, kSbf1Hpf = 0x48, kSbf1Sub = 0x50, kSbf2Detune = 0x1c;
// [FW] D's: the gain, the HPF's or Q's and the LPF's slews, the third word (the first's Sub slew, the second's level),
// the octave's factor, the first's level, the Own depths' slews, the new delay, the fractions, the crossfade's progress
constexpr size_t kSbfGain = 0xe4, kSbfSlews = 0xe8, kSbfThird = 0xf0, kSbfOctave = 0xf4, kSbf1Level = 0xf8;
constexpr size_t kSbfOwn = 0x124, kSbfDelay = 0x130, kSbfFraction = 0x134, kSbf1Fraction = 0x138, kSbfProgress = 0x13c;
constexpr size_t kSbfDcLeft = 0x90, kSbfDcRight = 0xc8;   // [FW] D+0x38, R's D+0x70: the DC follower's rate

XtSbfBlock sbfBlockOf(const Vc2CosmBlockWords& b) {
    const bool second = static_cast<int8_t>(b.code) == kSbf2;
    XtSbfBlock x = second ? floatsOf(b, kSbf2Floats) : floatsOf(b, kSbf1Floats);
    x.second = second;
    x.octave = static_cast<int8_t>(b.words[0]);
    return x;
}

// XtCosmSbf's note-on on the bound start levels and the held levels, as rom.4's start values and new block leave them
void sbfNoteOn(XtCosmSbf& sbf, Vc2CosmNoteOn& n, const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    const Vc2CosmStarts s = startsOf(n, o), h = heldOf(o);
    sbf.noteOn(sbfBlockOf(b), o.setUp.pitch, o.setUp.other, s.envA, h.envA, 0.0f, s.lfoA, s.envB, h.envB, 0.0f, s.lfoB);
}

void floatInto(Vc2CosmObjectWords& o, size_t at, float v) { floatsInto(o, at, &v, 1); }
} // namespace

XtCompBlock Vc2CosmAdapter::compBlockOf(const Vc2CosmBlockWords& b) { return floatsOf(b, kCompFloats); }
XtFsBlock Vc2CosmAdapter::fsBlockOf(const Vc2CosmBlockWords& b) { return floatsOf(b, kFsFloats); }
XtLimBlock Vc2CosmAdapter::limBlockOf(const Vc2CosmBlockWords& b) {
    XtLimBlock x = floatsOf(b, kLimFloats);
    x.ratio = static_cast<int8_t>(b.words[0]);
    return x;
}

// The start values: the delay from Detune and the note's pitch, and the Width, into the object's two values
void vc2CosmSbfStartValues(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmSbf& sbf = n.scratch().sbf;
    sbfNoteOn(sbf, n, o, b);
    o.values[0] = sbf.delay();
    o.values[1] = sbf.width();
}

// The new block: XtCosmSbf's targets on the held levels and the octave's factor; the Octave's byte and the block's
// words into the object's, rom.4's own, the ratios XtCosmSbf forms in their places
void vc2CosmSbfNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmSbf& sbf = n.scratch().sbf;
    const XtSbfBlock x = sbfBlockOf(b);
    targetsOn(sbf, x, o, &XtCosmSbf::detuneTarget, &XtCosmSbf::widthTarget);
    o.variant = static_cast<uint8_t>(b.words[0]);
    floatInto(o, kSbfOctave, sbf.octaveFactor());
    if (x.second) {
        copyBlock(o, b, 8, 4);
        floatInto(o, kSbf2Detune, sbf.detuneRatio());
        copyBlock(o, b, 0x10, 0x34);
        return;
    }
    floatInto(o, kSbf1Detune, sbf.detuneRatio());
    copyBlock(o, b, 0xc, 0x2c);
    floatInto(o, kSbf1Hpf, sbf.hpfHalf());
    copyBlock(o, b, 0x3c, 4);
    floatInto(o, kSbf1Sub, sbf.subRatio());
    copyBlock(o, b, 0x44, 4);
}

// XtCosmSbf's update's sum on the object's words from `at`: the value, its Env depth a word on, its LFO depth three on
float sbfTarget(const Vc2CosmBlockWords& w, int at, float env, float lfo) {
    const float withEnv = xtf::add(xtf::mul(blockFloat(w, at + 1), env), blockFloat(w, at));
    return xtf::add(xtf::mul(blockFloat(w, at + 3), lfo), withEnv);
}

// The update's case: the two sums, on the object's words as the block's Detune has become its ratio there, which no
// block of XtCosmSbf's can give it: the Detune's from word 1 (the second filter's 2), the Width's six words on
void vc2CosmSbfUpdate(Vc2CosmNoteOn&, Vc2CosmObjectWords& o) {
    const Vc2CosmBlockWords w = blockIn(o);
    const Vc2CosmStarts h = heldOf(o);
    const int at = w.code == kSbf2 ? 2 : 1;
    o.targets[0] = sbfTarget(w, at, h.envA, h.lfoA);
    o.targets[1] = sbfTarget(w, at + 6, h.envB, h.lfoB);
}

// The update's case: XtCosmFs's targets, the same as its new block's, on the block the object holds
void vc2CosmFsUpdate(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o) {
    targetsOn(n.scratch().fs, floatsOf(blockIn(o), kFsFloats), o, &XtCosmFs::depthTarget, &XtCosmFs::balanceTarget);
}

// The note-on case: what XtCosmSbf's note-on ends with: the slews at the block's, the gain from the Width's target, the
// new delay and its fractions, the crossfade done, the level 0 and the DC follower's rate
void vc2CosmSbfNoteOnCase(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    XtCosmSbf& sbf = n.scratch().sbf;
    sbfNoteOn(sbf, n, o, b);
    const bool second = static_cast<int8_t>(b.code) == kSbf2;
    floatInto(o, kSbfGain, sbf.gain());
    floatsInto(o, kSbfSlews, sbf.slews(), 2);
    floatInto(o, kSbfThird, second ? sbf.level() : sbf.slews()[2]);
    floatInto(o, kSbfOctave, sbf.octaveFactor());
    floatsInto(o, kSbfOwn, sbf.slews() + 3, 2);
    floatInto(o, kSbfDelay, sbf.newDelay());
    floatInto(o, kSbfFraction, sbf.fractions()[0]);
    floatInto(o, kSbfProgress, sbf.progress());
    floatInto(o, kSbfDcLeft, sbf.dcRate());
    floatInto(o, kSbfDcRight, sbf.dcRate());
    if (second) return;
    floatInto(o, kSbf1Level, sbf.level());
    floatInto(o, kSbf1Fraction, sbf.fractions()[1]);
}

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
    targetsOn(n.scratch().fs, floatsOf(b, kFsFloats), o, &XtCosmFs::depthTarget, &XtCosmFs::balanceTarget);
    copyBlock(o, b, 4, kFsBytes);
}

// The note-on case: the phase cleared, rom.4's own
void vc2CosmFsNoteOnCase(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords&) {
    const float cleared = 0.0f;
    floatsInto(o, kFsPhase, &cleared, 1);
}
