// The VC-2 part's notes' Matrix objects: see Vc2NoteMatrix.h (v280).  Each routine's reads and writes in rom.4's order
// where they can meet.
#include "Vc2NoteMatrix.h"

namespace {
constexpr int32_t kBendCentre = 8192;                       // [FW] the channel's bend at rest
constexpr std::array<uint32_t, 5> kControls { 7, 11, 10, 93, 91 };   // [FW] the controllers M keeps, in its order
constexpr std::size_t kHalfSlots = 8;                       // [FW] kind 0 clears the second eight slots, else the first

// The channel's bend by the Bend Ranges: above the centre by Up, below by Down, each × 8 / 65536 of the product
int32_t bendBy(int16_t raw, uint16_t up, uint16_t down) {
    const int16_t v = static_cast<int16_t>(raw - kBendCentre);
    if (v == 0) return 0;
    if (v > 0) return static_cast<int32_t>((static_cast<uint32_t>(v) * up << 3) >> 16);
    const uint32_t below = static_cast<uint16_t>(-static_cast<int32_t>(v));
    return -static_cast<int32_t>((below * down << 3) >> 16);
}

uint16_t halfAt(const uint8_t* bytes) { return static_cast<uint16_t>(bytes[0] | bytes[1] << 8); }
} // namespace

void Vc2NoteMatrices::fromPart(Vc2NoteMatrixWords& w, uint8_t part, uint32_t kind, const Vc2SetMatrixIn& in,
                               Vc2SetReads& reads) {
    const Vc2MatrixFront& rec = in.g.parts[part];
    const bool main = kind == 0;
    w.links.part = part;
    w.bendUp = main ? halfAt(rec.head) : 0;
    w.bendDown = main ? halfAt(rec.head + 2) : 0;
    vc2MatrixFromRecord(w.m, rec);
    const std::size_t from = main ? kHalfSlots : 0;
    for (std::size_t k = from; k < from + kHalfSlots; ++k) w.m.slots[k].dest = 0;
    const uint32_t channel = w.links.channel;
    w.bend = bendBy(reads.bendOf(channel), w.bendUp, w.bendDown);
    for (std::size_t k = 0; k < kControls.size(); ++k) w.controls[k] = reads.controlOf(channel, kControls[k]);
    w.cleared.fill(0);
}

void Vc2NoteMatrices::construct(uint32_t m, const Vc2SetCall& call, uint32_t kind, const Vc2SetMatrixIn& in,
                                Vc2SetReads& reads) {
    Vc2NoteMatrixWords& w = at(m);
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    w.links.channel = reads.channelOf(v.keys);
    const uint8_t part = reads.firstByte(v.part);
    w.m.head[0] = kind == 0 ? v.byte43 : 0;
    w.m.head[1] = v.key;
    w.m.head[2] = v.byte42;
    vc2MatrixHead(in.partM, in.partLinks, part, { w.m.head[0], w.m.head[1], w.m.head[2] });
    fromPart(w, part, kind, in, reads);
}

void Vc2NoteMatrices::update(uint32_t m, const Vc2SetCall& call, uint32_t kind, const Vc2SetMatrixIn& in,
                             Vc2SetReads& reads) {
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    fromPart(at(m), reads.firstByte(v.part), kind, in, reads);
}
