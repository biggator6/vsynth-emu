// A VC-2 COSM's sound: see Vc2CosmDsp.h (v263).  Every address is rom.4's, every offset the object's.
#include "Vc2CosmDsp.h"
#include "Vc2CosmAdapterBlocks.h"
#include <span>

using namespace Vc2CosmAdapter;

namespace {
constexpr uint32_t kStartLevel = 0x58, kRate = 0x54;   // [FW] a bound envelope's start level and rate
constexpr int32_t kLastCode = 20;                      // [FW] the new block's switch

// The bound envelopes' start levels and rates, and the levels the frame has held
Vc2CosmDspLevels levelsOf(const Vc2CosmObjectWords& o, Vc2CosmBound& bound) {
    return { bound.word(o.bindings[0] + kStartLevel), o.held[0], bound.word(o.bindings[0] + kRate), o.held[2],
             bound.word(o.bindings[1] + kStartLevel), o.held[1], bound.word(o.bindings[1] + kRate), o.held[3] };
}

// A type with two values, each on its envelope and LFO
template <auto Member, auto BlockOf>
struct TwoValues {
    static void noteOn(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
        (s.*Member).noteOn(BlockOf(b), l.startA, l.heldA, l.rateA, l.lfoA, l.startB, l.heldB, l.rateB, l.lfoB);
    }
    static void newBlock(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
        (s.*Member).setBlock(BlockOf(b), l.heldA, l.lfoA, l.heldB, l.lfoB);
    }
    static void update(XtCosmSection& s, const Vc2CosmDspLevels& l) {
        (s.*Member).update(l.heldA, l.rateA, l.lfoA, l.heldB, l.rateB, l.lfoB);
    }
};

// The Amp Simulator: one value, on envelope A and LFO A
struct AmpValue {
    static void noteOn(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
        s.amp.noteOn(ampBlockOf(b), l.startA, l.heldA, l.rateA, l.lfoA);
    }
    static void newBlock(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
        s.amp.setBlock(ampBlockOf(b), l.heldA, l.lfoA);
    }
    static void update(XtCosmSection& s, const Vc2CosmDspLevels& l) { s.amp.update(l.heldA, l.rateA, l.lfoA); }
};

// A type with no envelope or LFO: the block alone, and no update
template <auto Member, auto BlockOf>
struct BlockAlone {
    static void noteOn(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels&) {
        (s.*Member).noteOn(BlockOf(b));
    }
    static void newBlock(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels&) {
        (s.*Member).setBlock(BlockOf(b));
    }
};

// [FW] the Speaker Simulator's block: its Speaker Type, the byte +4
XtSpkBlock spkBlockOf(const Vc2CosmBlockWords& b) {
    XtSpkBlock x;
    x.type = static_cast<uint8_t>(b.words[0]);
    return x;
}

using Od = TwoValues<&XtCosmSection::od, odBlockOf>;
using Ws = TwoValues<&XtCosmSection::ws, wsBlockOf>;
using Res = TwoValues<&XtCosmSection::res, resBlockOf>;
using Dual = TwoValues<&XtCosmSection::dual, dualBlockOf>;
using Tvf = TwoValues<&XtCosmSection::tvf, tvfBlockOf>;
using Fs = TwoValues<&XtCosmSection::fs, fsBlockOf>;
using Lofi = TwoValues<&XtCosmSection::lofi, lofiBlockOf>;
using Spk = BlockAlone<&XtCosmSection::spk, spkBlockOf>;
using Comp = BlockAlone<&XtCosmSection::comp, compBlockOf>;
using Lim = BlockAlone<&XtCosmSection::lim, limBlockOf>;

// Each block code's type, by XtCosmSection's numbers
constexpr Vc2CosmDspRow kRows[] { { 1, 13, Od::noteOn, Od::newBlock, Od::update },
                                  { 2, 13, Od::noteOn, Od::newBlock, Od::update },
                                  { 3, 12, Ws::noteOn, Ws::newBlock, Ws::update },
                                  { 4, 14, AmpValue::noteOn, AmpValue::newBlock, AmpValue::update },
                                  { 5, 15, Spk::noteOn, Spk::newBlock, nullptr },
                                  { 6, 7, Res::noteOn, Res::newBlock, Res::update },
                                  { 12, 2, Dual::noteOn, Dual::newBlock, Dual::update },
                                  { 13, 0, Tvf::noteOn, Tvf::newBlock, Tvf::update },
                                  { 15, 10, Comp::noteOn, Comp::newBlock, nullptr },
                                  { 16, 11, Lim::noteOn, Lim::newBlock, nullptr },
                                  { 18, 8, Fs::noteOn, Fs::newBlock, Fs::update },
                                  { 19, 9, Lofi::noteOn, Lofi::newBlock, Lofi::update } };

const Vc2CosmDspRow* rowOf(int32_t code) {
    for (const Vc2CosmDspRow& r : std::span(kRows))
        if (r.code == code) return &r;
    return nullptr;
}
} // namespace

void Vc2CosmDsp::noteOn(const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, Vc2CosmBound& bound, bool stereo) {
    row_ = rowOf(static_cast<int8_t>(o.dspCode));
    if (row_ == nullptr) return;
    section_.type = row_->type;
    section_.stereo = stereo;
    row_->noteOn(section_, b, levelsOf(o, bound));
}

void Vc2CosmDsp::newBlock(const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, Vc2CosmBound& bound) {
    if (row_ == nullptr || o.code != b.code || static_cast<uint32_t>(o.code) > kLastCode) return;
    row_->newBlock(section_, b, levelsOf(o, bound));
}

void Vc2CosmDsp::update(const Vc2CosmObjectWords& o, Vc2CosmBound& bound) {
    if (row_ != nullptr && row_->update != nullptr) row_->update(section_, levelsOf(o, bound));
}

void Vc2CosmDsp::render(float* lr) {
    if (row_ != nullptr) section_.process(lr, 1.0f);
}
