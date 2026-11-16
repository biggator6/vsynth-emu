// The VC-2's tone object's frame: see Vc2Tone.h (v228).  Each routine's calls in rom.4's order; every address is rom.4's.
#include "Vc2Tone.h"
#include <numeric>

namespace {
using Piece = Vc2TonePiece;
// [FW] the parts in the order 0x893b2db0 builds them
constexpr Piece kBuildOrder[] = { Piece::osc1, Piece::osc2, Piece::cosm1, Piece::cosm2,
                                  Piece::tva, Piece::mix, Piece::envelopes, Piece::lfos };
// [FW] and 0x893b2fc0 takes them down (after the following LFOs and the lists)
constexpr Piece kDestroyOrder[] = { Piece::lfos, Piece::cosm2, Piece::cosm1 };
constexpr int32_t kMixKind = 3;          // [FW] 0x893b4088: the OSC 1 kind the mix's install is told of
constexpr int32_t kAnyValue = -1;        // [FW] 0x893b4caa: the matched count's value that matches every tone
} // namespace

void Vc2Tone::construct(uint32_t self, uint32_t id, Vc2ToneParts& parts) {
    head.next = head.prev = 0;   // the base's list node, 0x893ad920
    for (Piece piece : kBuildOrder) parts.build(piece);
    tail.zeroed[0] = tail.zeroed[1] = 0;
    head.vtable = kVtable;
    head.self = self;
    head.state = kBuilt;
    head.id = id;
    tail.renders = 0;
    std::iota(envelopeIndex.begin(), envelopeIndex.end(), 0);
    std::iota(lfoIndex.begin(), lfoIndex.end(), 0);
    tail.index = head.index;
}

void Vc2Tone::destroy(Vc2ToneParts& parts) {
    head.vtable = kVtable;
    for (int lfo = 1; lfo < kLfos; ++lfo) {   // LFO 0 is never taken off a list
        if (parts.lfoFollows(lfo)) parts.unfollow(lfo);
    }
    parts.unlist(head.state == kFromOne);
    for (Piece piece : kDestroyOrder) parts.destroy(piece);
}

void Vc2Tone::update(int32_t n, Vc2ToneParts& parts) {
    if (head.state != kSounding) return;
    const auto ticks = static_cast<int32_t>(static_cast<int16_t>(n));   // [FW] 0x893b4964: exts.w
    for (int envelope = 1; envelope < kEnvelopes; ++envelope) {        // envelope 0 is never ticked
        if (parts.envelopeOn(envelope)) parts.tickEnvelope(envelope, ticks);
    }
    for (int lfo = 1; lfo < kLfos; ++lfo) {   // nor LFO 0
        if (parts.lfoOn(lfo)) parts.tickLfo(lfo, ticks);
    }
    parts.updateOsc(kVc2ToneOsc1, ticks);
    parts.updateOsc(kVc2ToneOsc2, ticks);
    parts.updateMix();
    parts.updateCosm(kVc2ToneCosm1, parts.oscValue(kVc2ToneOsc1));
    if (!parts.tvaOver() && !parts.tvaStopped()) {
        parts.updateTva();
        return;
    }
    head.state = kEnded;
    parts.ended();
}

void Vc2Tone::renderWhole(float* out, Vc2ToneBuffers& b, Vc2ToneParts& parts) {
    if (tail.renders == 0) return;
    parts.renderOsc(kVc2ToneOsc2, b.b);
    parts.renderOsc(kVc2ToneOsc1, b.a);
    parts.renderMix(b.a, b.b, b.c);
    parts.renderCosm(kVc2ToneCosm1, b.c);
    parts.renderTva(b.c, out);
}

void Vc2Tone::render(float* out, Vc2ToneBuffers& b, Vc2ToneParts& parts) {
    if (tail.renders == 0) return;
    parts.renderOsc(kVc2ToneOsc1, b.a);
    parts.renderTva(b.a, out);
}

void Vc2Tone::clock(float x, Vc2ToneParts& parts) {
    for (int lfo = 0; lfo < kLfos; ++lfo) parts.clockLfo(lfo, x);
}

void Vc2Tone::count(uint32_t x, Vc2ToneParts& parts) {
    parts.countOsc(kVc2ToneOsc1, x);
    parts.countOsc(kVc2ToneOsc2, x);
}

void Vc2Tone::countMatched(uint32_t x, int32_t value, uint32_t mask, Vc2ToneParts& parts) {
    if (value != kAnyValue && (head.id & mask) != static_cast<uint32_t>(value)) return;
    parts.countOscMatched(kVc2ToneOsc1, x);
    parts.countOscMatched(kVc2ToneOsc2, x);
}

bool Vc2Tone::sounds(Vc2ToneParts& parts) const { return parts.tvaLevel() > 0.0f; }

bool Vc2Tone::held(Vc2ToneParts& parts) const { return parts.tvaHeld(); }

void Vc2Tone::start() { head.state = kSounding; }

void Vc2Tone::install(Vc2ToneParts& parts) {
    parts.installOsc(kVc2ToneOsc1);
    const uint32_t flag = parts.oscFlag(kVc2ToneOsc1);   // read in the next call's delay slot, before OSC 2's install
    parts.installOsc(kVc2ToneOsc2);
    parts.installMix(parts.oscKind(kVc2ToneOsc1) == kMixKind);
    const uint32_t given = parts.installCosm(kVc2ToneCosm1, flag);   // COSM 2 is given what COSM 1's install returns
    parts.installCosm(kVc2ToneCosm2, given);
}

Vc2Tone* Vc2TonePool::make(uint32_t id, Vc2ToneParts& parts) {
    if (head == end()) return nullptr;
    if (head == 0) {   // the first take links the pool: each tone to the next, the last to the end
        head = addressOf(0);
        for (int i = 0; i < kTones; ++i) {
            tones[(size_t)i].link = addressOf(i + 1);
            tones[(size_t)i].head.serial = tones[(size_t)i].head.index = i;
        }
    }
    const uint32_t address = head;
    Vc2Tone& tone = at(address);
    tone.head.serial = serial++;
    head = tone.link;
    tone.construct(address, id, parts);
    return &tone;
}

void Vc2TonePool::unmake(Vc2Tone& tone, bool back, Vc2ToneParts& parts) {
    tone.destroy(parts);
    if (!back) return;
    tone.link = head;
    head = addressOf(static_cast<int>(&tone - tones.data()));   // the tone given, as rom.4's `this`
}
