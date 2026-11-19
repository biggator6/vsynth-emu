// The VC-2 tone's note-on, 0x893b3390 with 0x893b4380 and 0x893b4660: see Vc2Tone.h (v229).  Each call in rom.4's order;
// every address is rom.4's.  HARDWARE_FIRMWARE.md, "The VC-2's tone's note-on".
#include "Vc2Tone.h"
#include <algorithm>

namespace {
using In = Vc2ToneInput;
constexpr int kNoTone = -1;
constexpr int32_t kNoPrevious = -1;      // [FW] 0x893b36c6: the input's serial for no legato
constexpr uint32_t kAnyRender = 0xffffffff;
// [FW] 0x8993fb14: the structure renders' member pointers {this-delta, −1, routine}: 0, 1, 2, then any other
constexpr std::array<std::array<uint32_t, 3>, 4> kRenders = { {
    { 0, kAnyRender, 0x893b40c0 }, { 0, kAnyRender, 0x893b4140 },
    { 0, kAnyRender, 0x893b4220 }, { 0, kAnyRender, 0x893b4330 },
} };
constexpr uint32_t kLastStructure = 2;   // [FW] 0x893b33d2: above it (unsigned), the last entry

// [FW] 0x893b4380: in an OSC's block, the offsets of its four envelopes' and four LFOs' numbers by its kind (+0); 0
// reads none (the number 0), and a kind not listed (0, 4, 7 on, below 0) reads none at all
struct OscBinds {
    int32_t kind;
    std::array<uint16_t, 4> envelopes, lfos;
};
constexpr OscBinds kOscBinds[] = {
    { 1, { 0x20, 0x28, 0x30, 0x38 }, { 0x40, 0x48, 0x50, 0x58 } },
    { 2, { 0x20, 0x28, 0x30, 0x38 }, { 0x40, 0x48, 0x50, 0x58 } },
    { 3, { 0x20, 0xbc, 0xd4, 0x38 }, { 0x40, 0xc4, 0xd8, 0x58 } },
    { 5, { 0x20, 0xec, 0x100, 0x38 }, { 0x40, 0xf0, 0x104, 0x58 } },
    { 6, { 0x20, 0, 0x30, 0x38 }, { 0, 0, 0, 0 } },
};
// [FW] 0x893b4660: in a COSM's block, its two envelopes' and two LFOs' offsets by its type (+0, 0…20); the same rules
struct CosmBinds {
    int32_t type;
    std::array<uint16_t, 2> envelopes, lfos;
};
constexpr CosmBinds kCosmBinds[] = {
    { 1, { 0xc, 0x20 }, { 0x14, 0x28 } },  { 2, { 0xc, 0x20 }, { 0x14, 0x28 } },  { 3, { 0x10, 0x24 }, { 0x18, 0x2c } },
    { 4, { 0x10, 0 }, { 0x18, 0 } },       { 6, { 0x10, 0x24 }, { 0x18, 0x2c } }, { 7, { 0x10, 0x28 }, { 0x18, 0x30 } },
    { 8, { 0x14, 0x2c }, { 0x1c, 0x34 } }, { 9, { 0x10, 0x20 }, { 0x18, 0x28 } }, { 10, { 0xc, 0x20 }, { 0x14, 0x28 } },
    { 12, { 0x14, 0x2c }, { 0x1c, 0x34 } }, { 13, { 0xc, 0x24 }, { 0x14, 0x2c } },
    { 14, { 0xc, 0x24 }, { 0x14, 0x2c } },  { 17, { 0x20, 0xc }, { 0x28, 0x14 } },
    { 18, { 0xc, 0x20 }, { 0x14, 0x28 } },  { 19, { 0x10, 0x24 }, { 0x18, 0x2c } },
    { 20, { 0x24, 0x34 }, { 0x2c, 0x3c } },
};
// [FW] 0x893b3694…0x893b36be: the input's fix-up, with the mix's type 4 and OSC 2's kind 3, its +0xb0 from 10 to 2
constexpr uint32_t kFixMix = 4, kFixKind = 3, kFixFrom = 10, kFixTo = 2;

// The numbers a part's block names at `offsets` (0 the number 0)
template <size_t N>
std::array<int32_t, N> numbersAt(const uint32_t* block, const std::array<uint16_t, N>& offsets) {
    std::array<int32_t, N> numbers {};
    for (size_t k = 0; k < N; ++k) {
        if (offsets[k] != 0) numbers[k] = static_cast<int32_t>(block[offsets[k] / sizeof(uint32_t)]);
    }
    return numbers;
}

// A part's binding by its block's +0, from its table's row, or none
template <size_t N, class Row, size_t R>
Vc2ToneBinding<N> bindingOf(const uint32_t* block, const Row (&rows)[R], int32_t Row::*key) {
    Vc2ToneBinding<N> b;
    const auto* row = std::ranges::find(rows, static_cast<int32_t>(block[0]), key);
    if (row == std::end(rows)) return b;
    b.envelopes = numbersAt(block, row->envelopes);
    b.lfos = numbersAt(block, row->lfos);
    return b;
}

// A tone of the same id on the lists: the first list, or the second when that is empty, each tone's +4 the next
int sameIdTone(uint32_t id, Vc2TonePool& pool, Vc2ToneOwner& owner) {
    const std::array<uint32_t, 2> heads = owner.lists();
    // [FW] the walk back ends at a list's head (v273, Vc2ToneLists.h): its +0x14 and +4 stay 0, so it names no tone
    for (uint32_t node = heads[0] != 0 ? heads[0] : heads[1]; node != 0 && pool.holds(node);
         node = pool.at(node).head.prev) {
        const uint32_t tone = pool.at(node).head.self;
        if (tone != 0 && pool.at(tone).head.id == id) return pool.indexOf(tone);
    }
    return kNoTone;
}

void noteOnLfos(Vc2Tone& t, const In& in, Vc2TonePool& pool, Vc2ToneOwner& owner, Vc2ToneParts& parts) {
    const int same = sameIdTone(t.head.id, pool, owner);
    const float x = in.floatAt(In::kLfoValue);
    for (int lfo = 1; lfo < Vc2Tone::kLfos; ++lfo) {   // LFO 0 is never started
        const uint32_t* block = in.block(In::kLfos + In::kLfoStride * (uint32_t)lfo);
        if (block[In::kKeySync / sizeof(uint32_t)] != 0) {
            parts.noteOnLfo(lfo, block, x);
            continue;
        }
        if (same != kNoTone) {
            parts.followLfo(lfo, same, block);
            continue;
        }
        const uint32_t free = parts.freeLfo(t.head.id, lfo);
        parts.attachFree(lfo, free);
        parts.startFree(lfo, block, parts.freeValue(free), x);
        if (parts.lfoListed(lfo)) {
            parts.relist(parts.lfoFollow(lfo), t.head.id, lfo);
        } else {
            parts.list(t.head.id, lfo);
        }
        parts.markFree(free, t.head.id, lfo);
    }
}

// The OSC voices' note-ons, each a legato that takes the player of the tone of the input's serial when the input asks
// and that tone's OSC has the block's kind and +0xb0
void noteOnOscs(const In& in, Vc2TonePool& pool, Vc2ToneOwner& owner, Vc2ToneParts& parts) {
    const auto previous = static_cast<int32_t>(in.at(In::kPrevious));
    const int from = previous == kNoPrevious ? kNoTone : pool.bySerial(previous);
    for (int osc = kVc2ToneOsc1; osc <= kVc2ToneOsc2; ++osc) {
        const uint32_t* block = in.block(In::kOscs[(size_t)osc]);
        const bool asks = previous != kNoPrevious && block[In::kLegato / sizeof(uint32_t)] != 0 && from != kNoTone;
        Vc2ToneParts* was = asks ? &owner.partsOf(from) : nullptr;
        const bool takes = was && static_cast<int32_t>(block[0]) == was->oscKind(osc)
                           && block[In::kWave / sizeof(uint32_t)] == was->oscWave(osc);
        parts.noteOnOsc(osc, block, takes);
        if (!takes) continue;
        parts.takePlayer(osc, from);
        parts.setOscWave(osc, was->oscWave(osc));
    }
}

// The COSMs' set-ups by the structure: 0 and 1 both from OSC 1, 2 each from its own OSC, any other none
void setUpCosms(uint32_t structure, const In& in, Vc2ToneParts& parts) {
    if (structure > kLastStructure) return;
    const int second = structure == kLastStructure ? kVc2ToneOsc2 : kVc2ToneOsc1;
    for (int cosm = kVc2ToneCosm1; cosm <= kVc2ToneCosm2; ++cosm) {
        const int osc = cosm == kVc2ToneCosm1 ? kVc2ToneOsc1 : second;
        parts.setUpCosm(cosm, parts.cosmSetUp(osc), parts.oscFlag(osc));
        parts.startCosm(cosm, in.block(In::kCosms[(size_t)cosm]), cosm);
    }
}
} // namespace

void Vc2Tone::noteOn(Vc2ToneInput& in, Vc2TonePool& pool, Vc2ToneOwner& owner, Vc2ToneParts& parts) {
    tail.renders = 0;
    tail.structure = static_cast<int32_t>(in.at(In::kStructure));
    const auto& render = kRenders[std::min((uint32_t)tail.structure, kLastStructure + 1)];
    std::ranges::copy(render, tail.render);
    for (int envelope = 1; envelope < kEnvelopes; ++envelope) {   // envelope 0 is never started
        parts.noteOnEnvelope(envelope, in.block(In::kEnvelopes + In::kEnvelopeStride * (uint32_t)envelope));
    }
    noteOnLfos(*this, in, pool, owner, parts);
    for (int osc = kVc2ToneOsc1; osc <= kVc2ToneOsc2; ++osc) {
        parts.bindOsc(osc, bindingOf<4>(in.block(In::kOscs[(size_t)osc]), kOscBinds, &OscBinds::kind));
    }
    for (int cosm = kVc2ToneCosm1; cosm <= kVc2ToneCosm2; ++cosm) {
        parts.bindCosm(cosm, bindingOf<2>(in.block(In::kCosms[(size_t)cosm]), kCosmBinds, &CosmBinds::type));
    }
    uint32_t& fixed = in.at(In::kOscs[kVc2ToneOsc2] + In::kWave);
    if (in.at(In::kMix) == kFixMix && in.at(In::kOscs[kVc2ToneOsc2]) == kFixKind && fixed == kFixFrom) fixed = kFixTo;
    noteOnOscs(in, pool, owner, parts);
    parts.setOsc(kVc2ToneOsc1, in.block(In::kOscSet));
    parts.setOsc(kVc2ToneOsc2, in.block(In::kOscSet));
    parts.noteOnMix(in.block(In::kMix), parts.oscFlag(kVc2ToneOsc1), parts.oscFlag(kVc2ToneOsc2));
    setUpCosms((uint32_t)tail.structure, in, parts);
    const auto number = [&](uint32_t offset) { return static_cast<int32_t>(in.at(offset)); };
    parts.bindTva(number(In::kTva), number(In::kTvaLfoA), number(In::kTvaLfoB), head.serial);
    parts.noteOnTva(in.block(In::kTva), parts.oscFlag(kVc2ToneOsc1));
    head.state = kFromOne;
    install(parts);
    tail.renders = 1;
    parts.enlist();
}

int Vc2TonePool::bySerial(int32_t wanted) const {
    const auto found = std::ranges::find_if(tones, [&](const Vc2Tone& t) { return t.head.serial == wanted; });
    return found == tones.end() ? -1 : (int)(found - tones.begin());
}
