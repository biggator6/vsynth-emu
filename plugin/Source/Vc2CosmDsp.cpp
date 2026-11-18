// A VC-2 COSM's sound: see Vc2CosmDsp.h (v263, v264).  Every address is rom.4's, every offset the object's.
#include "Vc2CosmDsp.h"
#include "Vc2CosmAdapterBlocks.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <span>

using namespace Vc2CosmAdapter;

namespace {
constexpr uint32_t kStartLevel = 0x58, kRate = 0x54;   // [FW] a bound envelope's start level and rate
constexpr int32_t kLastCode = 20;                      // [FW] the new block's switch
constexpr int kGrowlType = -1;                         // the Growl is no type of XtCosmSection's
constexpr int kRingFloats = 3266;                      // [FW] the Compressor's, Limiter's and Shifter's rings
static_assert(XtCosmSbf::kGuard == Vc2CosmPools::kEarlyFloats, "the same early words");

// The bound envelopes' start levels and rates, the levels the frame has held, the note's pitch and root
Vc2CosmDspLevels levelsOf(const Vc2CosmObjectWords& o, Vc2CosmBound& bound, float word98 = 0.0f) {
    return { bound.word(o.bindings[0] + kStartLevel), o.held[0], bound.word(o.bindings[0] + kRate), o.held[2],
             bound.word(o.bindings[1] + kStartLevel), o.held[1], bound.word(o.bindings[1] + kRate), o.held[3],
             o.setUp.pitch, o.setUp.other, word98 };
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

// The Side Band Filters: their delays follow the note's pitch and root
struct SideBand {
    static void noteOn(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
        s.sbf.noteOn(sbfBlockOf(b), l.pitch, l.root, l.startA, l.heldA, l.rateA, l.lfoA, l.startB, l.heldB, l.rateB,
                     l.lfoB);
    }
    static void newBlock(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
        s.sbf.setBlock(sbfBlockOf(b), l.heldA, l.lfoA, l.heldB, l.lfoB);
    }
    static void update(XtCosmSection& s, const Vc2CosmDspLevels& l) {
        s.sbf.update(l.pitch, l.heldA, l.rateA, l.lfoA, l.heldB, l.rateB, l.lfoB);
    }
};

// The TB Filter: no start levels or rates, and R's second HPF stage starts from what the last note left in its words
struct TbFilter {
    static void noteOn(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
        s.tb.noteOn(tbBlockOf(b), l.heldA, l.lfoA, l.heldB, l.lfoB);
        s.tb.setHpWords(s.word8c, s.word90);
    }
    static void newBlock(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
        s.tb.setBlock(tbBlockOf(b), l.heldA, l.lfoA, l.heldB, l.lfoB);
    }
    static void update(XtCosmSection& s, const Vc2CosmDspLevels& l) { s.tb.update(l.heldA, l.lfoA, l.heldB, l.lfoB); }
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

// The Dynamic TVF: the TVF's, its follower starting from what the last note left in D+0x98
void dynamicNoteOn(XtCosmSection& s, const Vc2CosmBlockWords& b, const Vc2CosmDspLevels& l) {
    Tvf::noteOn(s, b, l);
    s.tvf.setFollower(s.tvf.qOwnSlewed(), s.tvf.cutoffOwnSlewed(), l.word98);
}

// The rings of a type with one a channel, at the slot's first and third buffers
template <auto Member>
Vc2CosmDspRings twoRings(const XtCosmSection& s) {
    return { { (s.*Member).buffer(0), nullptr, (s.*Member).buffer(1), nullptr }, kRingFloats };
}
Vc2CosmDspRings sideBandRings(const XtCosmSection& s) {
    return { { s.sbf.buffer(0), s.sbf.buffer(1), s.sbf.buffer(2), s.sbf.buffer(3) }, Vc2CosmPools::kBufferFloats };
}

// What each type leaves in D+0x98 (the Limiter's R gain, which a mono note leaves as its note-on set it)
float ampWord98(const XtCosmSection& s) { return s.amp.slews()[3]; }
float compWord98(const XtCosmSection& s) { return s.comp.gainTarget(0); }
float limWord98(const XtCosmSection& s) { return s.lim.gain(1); }
float lofiWord98(const XtCosmSection& s) { return s.lofi.upScale(); }
float dynamicWord98(const XtCosmSection& s) { return s.tvf.level(); }
float firstBandWord98(const XtCosmSection& s) { return s.sbf.slews()[2]; }
float secondBandWord98(const XtCosmSection& s) { return s.sbf.level(); }

// Each block code's type, by XtCosmSection's numbers
constexpr Vc2CosmDspRow kRows[] {
    { 1, 13, Od::noteOn, Od::newBlock, Od::update },
    { 2, 13, Od::noteOn, Od::newBlock, Od::update },
    { 3, 12, Ws::noteOn, Ws::newBlock, Ws::update },
    { 4, 14, AmpValue::noteOn, AmpValue::newBlock, AmpValue::update, nullptr, ampWord98 },
    { 5, 15, Spk::noteOn, Spk::newBlock, nullptr },
    { 6, 7, Res::noteOn, Res::newBlock, Res::update },
    { 7, 5, SideBand::noteOn, SideBand::newBlock, SideBand::update, sideBandRings, firstBandWord98 },
    { 8, 6, SideBand::noteOn, SideBand::newBlock, SideBand::update, sideBandRings, secondBandWord98 },
    { Vc2CosmGrowl::kCode, kGrowlType, nullptr, nullptr, nullptr },
    { 12, 2, Dual::noteOn, Dual::newBlock, Dual::update },
    { 13, 0, Tvf::noteOn, Tvf::newBlock, Tvf::update },
    { 14, 1, dynamicNoteOn, Tvf::newBlock, Tvf::update, nullptr, dynamicWord98 },
    { 15, 10, Comp::noteOn, Comp::newBlock, nullptr, twoRings<&XtCosmSection::comp>, compWord98 },
    { 16, 11, Lim::noteOn, Lim::newBlock, nullptr, twoRings<&XtCosmSection::lim>, limWord98 },
    { 18, 8, Fs::noteOn, Fs::newBlock, Fs::update, twoRings<&XtCosmSection::fs> },
    { 19, 9, Lofi::noteOn, Lofi::newBlock, Lofi::update, nullptr, lofiWord98 },
    { 20, 3, TbFilter::noteOn, TbFilter::newBlock, TbFilter::update },
};

const Vc2CosmDspRow* rowOf(int32_t code) {
    for (const Vc2CosmDspRow& r : std::span(kRows))
        if (r.code == code) return &r;
    return nullptr;
}
} // namespace

// The words a note can inherit from the one before: the object's own until a note has been here, then what that note's
// sound left in them
void Vc2CosmDsp::carryWords(const Vc2CosmObjectWords& o) {
    if (!noted_) {
        section_.word8c = std::bit_cast<float>(o.d0e4[0]);
        section_.word90 = std::bit_cast<float>(o.d0e4[1]);
        word98_ = std::bit_cast<float>(o.d0e4[3]);
        noted_ = true;
        return;
    }
    section_.holdWords();
    if (row_ != nullptr && row_->word98 != nullptr) word98_ = row_->word98(section_);
}

void Vc2CosmDsp::noteOn(const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, Vc2CosmBound& bound, bool stereo) {
    carryWords(o);
    row_ = rowOf(static_cast<int8_t>(o.dspCode));
    if (row_ == nullptr) return;
    section_.stereo = stereo;
    if (row_->type == kGrowlType) {
        std::ranges::fill(growl_.left, 0.0f);
        std::ranges::fill(growl_.right, 0.0f);
        return;
    }
    section_.type = row_->type;
    row_->noteOn(section_, b, levelsOf(o, bound, word98_));
}

void Vc2CosmDsp::newBlock(const Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, Vc2CosmBound& bound) {
    if (row_ == nullptr || o.code != b.code || static_cast<uint32_t>(o.code) > kLastCode) return;
    if (row_->newBlock != nullptr) row_->newBlock(section_, b, levelsOf(o, bound));
}

void Vc2CosmDsp::update(const Vc2CosmObjectWords& o, Vc2CosmBound& bound) {
    if (row_ != nullptr && row_->update != nullptr) row_->update(section_, levelsOf(o, bound));
}

Vc2CosmDspRings Vc2CosmDsp::rings() const {
    if (row_->type == kGrowlType)
        return { { growl_.left.data(), nullptr, growl_.right.data(), nullptr }, Vc2CosmGrowl::kLine };
    return row_->rings != nullptr ? row_->rings(section_) : Vc2CosmDspRings {};
}

void Vc2CosmDsp::render(Vc2CosmObjectWords& o, float* lr, const float* past) {
    if (row_ == nullptr) return;
    const bool slotted = o.slot >= 0;
    if (slotted && row_->rings == sideBandRings) {
        section_.sbf.setEarly(1, pools_.earlyOf(o.pool, o.slot, 1));
        section_.sbf.setEarly(3, pools_.earlyOf(o.pool, o.slot, 3));
    }
    if (row_->type == kGrowlType) {
        std::memcpy(&growl_.w, &o, sizeof o);
        growl_.render(lr);
        std::memcpy(&o, &growl_.w, sizeof o);
    } else {
        section_.process(lr, 1.0f, past);
    }
    if (!slotted) return;
    const Vc2CosmDspRings kept = rings();
    for (int k = 0; k < static_cast<int>(kept.lines.size()); ++k)
        if (kept.lines[(size_t)k] != nullptr) pools_.keepEnd(o.pool, o.slot, k, kept.lines[(size_t)k], kept.length);
}
