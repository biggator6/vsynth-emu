// The VC-2 part's conversion sets: see Vc2Sets.h (v280).  Each routine's calls in rom.4's order.
#include "Vc2Sets.h"
#include "Vc2ToneInput.h"

namespace {
using In = Vc2ToneInput;
constexpr uint32_t kFirstChild = 0x04;   // [FW] C's first child's pointer
constexpr uint32_t kOscStride = In::kOscs[1] - In::kOscs[0], kCosmStride = In::kCosms[1] - In::kCosms[0];

// [FW] by kind: allocator, constructor, end, conversion, update, copy, release, refresh; the allocator's bytes, C's
// slot, the count, the first block in W (C's envelopes and LFOs are W's from the second) and the next's
constexpr Vc2SetChild kChildren[] = {
    { 0x893a1940, 0x893a1960, 0x893a19b0, 0x893a19c0, 0x893a19e0, 0x893a1a10, 0, 0, 10, 0x04, 1, In::kOscSet, 0 },
    { 0x8939fdd0, 0x8939fe00, 0x893a0000, 0x893a0040, 0x893a0250, 0x893a0310, 0, 0x893a0150, 0x108, 0x08, 2,
      In::kOscs[0], kOscStride },
    { 0x8939bd90, 0x8939bdb0, 0x8939be30, 0x8939be40, 0x8939be60, 0x8939bec0, 0, 0, 32, 0x10, 1, In::kMix, 0 },
    { 0x8939a620, 0x8939a690, 0x8939a780, 0x8939a790, 0x8939a7b0, 0x8939a800, 0x8939a810, 0, 28, 0x14, 13,
      In::kEnvelopes + In::kEnvelopeStride, In::kEnvelopeStride },
    { 0x8939b690, 0x8939b6c0, 0x8939b740, 0x8939b750, 0x8939b770, 0x8939b7c0, 0, 0, 20, 0x48, 5,
      In::kLfos + In::kLfoStride, In::kLfoStride },
    { 0x89399ae0, 0x89399b10, 0x89399b80, 0x89399b90, 0x89399bb0, 0x89399c00, 0, 0, 60, 0x5c, 2, In::kCosms[0],
      kCosmStride },
    { 0x893a67d0, 0x893a67f0, 0x893a68b0, 0x893a68c0, 0x893a6940, 0x893a69a0, 0, 0, 40, 0x64, 1, In::kTva, 0 },
};
constexpr std::size_t kCommon = 0, kOsc = 1, kEnvelopes = 3;

bool isCommon(const Vc2SetChild& row) { return &row == &kChildren[kCommon]; }

uint32_t& childSlot(Vc2SetWords& c, const Vc2SetChild& row, uint32_t i) {
    return c.children[(row.slot - kFirstChild) / sizeof(uint32_t) + i];
}

uint32_t childBlock(const Vc2SetChild& row, uint32_t note, uint32_t i) {
    return note + row.noteAt + row.noteStride * i;
}

// Each child of C's in rom.4's order, `pass` given its row, its pointer in C and its index
template <class Pass>
void eachChild(Vc2SetWords& c, Pass pass) {
    for (const Vc2SetChild& row : kChildren)
        for (uint32_t i = 0; i < row.count; ++i) pass(row, childSlot(c, row, i), i);
}

// A pass that reaches every child but a common of none, which alone is tested (the conversion, update and copy)
template <class Call>
void eachLive(Vc2SetWords& c, Call call) {
    eachChild(c, [&](const Vc2SetChild& row, uint32_t child, uint32_t i) {
        if (!isCommon(row) || child != 0) call(row, child, i);
    });
}
} // namespace

std::span<const Vc2SetChild> Vc2Sets::children() { return kChildren; }

void Vc2Sets::make(uint32_t set, const Vc2SetCall& call, uint32_t note, Vc2Notes& notes, const Vc2SetMatrixIn& in,
                   Vc2SetParts& parts) {
    Vc2SetWords& c = at(set);
    Vc2NoteWords& w = notes.at(note);
    const Vc2SetVoice v = parts.voiceAt(call.channelAt);
    c.kind = w.kind;
    c.mode = parts.modeOf(parts.firstByte(v.part), c.kind);
    w.input.at(In::kStructure) = c.mode;
    const uint32_t m = matrices.addressOf(v.number);
    if (m != 0) matrices.construct(m, call, c.kind, in, parts);
    c.matrix = m;
    eachChild(c, [&](const Vc2SetChild& row, uint32_t& child, uint32_t i) {
        if (isCommon(row) && c.kind != 0) {
            child = 0;
            w.input.at(In::kOscSet) = 0;
            return;
        }
        const uint32_t made = parts.allocate(row, v.number, i);
        if (made != 0) parts.construct(row, { made, call, childBlock(row, note, i), c.matrix, c.kind, i });
        child = made;
    });
}

void Vc2Sets::end(uint32_t set, Vc2SetParts& parts) {
    if (set == 0) return;
    eachChild(at(set), [&](const Vc2SetChild& row, uint32_t child, uint32_t) {
        if (child != 0) parts.end(row, child);
    });
}

void Vc2Sets::convert(uint32_t set, const Vc2SetCall& call, uint32_t note, Vc2Notes& notes, Vc2SetParts& parts) {
    Vc2SetWords& c = at(set);
    notes.at(note).input.at(In::kStructure) = c.mode;
    eachLive(c, [&](const Vc2SetChild& row, uint32_t child, uint32_t i) {
        parts.convert(row, child, call, childBlock(row, note, i));
    });
}

void Vc2Sets::release(uint32_t set, uint32_t velocity, uint32_t note, Vc2SetParts& parts) {
    const Vc2SetChild& row = kChildren[kEnvelopes];
    for (uint32_t i = 0; i < row.count; ++i) {
        const uint32_t child = childSlot(at(set), row, i);
        if (child != 0) parts.release(row, child, static_cast<uint8_t>(velocity), childBlock(row, note, i));
    }
}

void Vc2Sets::update(uint32_t set, const Vc2SetCall& call, const Vc2SetMatrixIn& in, Vc2SetParts& parts) {
    Vc2SetWords& c = at(set);
    const Vc2SetVoice v = parts.voiceAt(call.channelAt);
    c.mode = parts.modeOf(parts.firstByte(v.part), c.kind);
    matrices.update(c.matrix, call, c.kind, in, parts);
    eachLive(c, [&](const Vc2SetChild& row, uint32_t child, uint32_t) { parts.update(row, child, call); });
}

void Vc2Sets::copy(uint32_t set, uint32_t note, Vc2Notes& notes, Vc2SetParts& parts) {
    Vc2SetWords& c = at(set);
    notes.at(note).input.at(In::kStructure) = c.mode;
    eachLive(c, [&](const Vc2SetChild& row, uint32_t child, uint32_t i) {
        parts.copy(row, child, childBlock(row, note, i));
    });
}

uint32_t Vc2Sets::refresh(uint32_t set, const Vc2SetCall& call, Vc2SetParts& parts) {
    const Vc2SetChild& row = kChildren[kOsc];
    Vc2SetWords& c = at(set);
    if (parts.refresh(row, childSlot(c, row, 0), call) != 0) return 1;
    return parts.refresh(row, childSlot(c, row, 1), call) != 0 ? 1 : 0;
}
