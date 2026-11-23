// The draws of VariPhrase's own routines (v331): see Vc2OscDraws.h, oscVariByEvents and the steers after it, which its
// note-on's range by events reads through.  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscDraws.h"
#include <algorithm>

namespace {
// [FW] VariPhrase's note-on's range by events (0x893af590): the block's flags (+0x138: 0x20 Vari off, 0x80 the range in
// samples), the wave types that go on to Vari off's words (1, 6, 7, 8), and the three events it reads (the range's
// start and end, the seek point); the object's events' table the lookup 0x893cc0d0 reads, its first half word and
// stride (the object's +0x10, +7, +8)
constexpr uint8_t kVariOffBit = 0x20, kInSamples = 0x80;
constexpr std::array<uint32_t, 4> kVariOffTypes { 1, 6, 7, 8 };
constexpr std::array<uint32_t, 3> kEvents { 0x13c, 0x140, 0x144 };
constexpr uint32_t kTable = 0x10, kFirst = 0x7, kStride = 0x8;

uint32_t heldEvent(uint32_t word, const OscEventTable& t) { return (word & 0xffff) % (uint32_t)(t.count + 2); }
} // namespace

bool oscVariByEvents(uint8_t flags, uint32_t type) {
    const bool variOff = (flags & kVariOffBit) != 0 && (flags & kInSamples) == 0;
    return variOff && std::ranges::find(kVariOffTypes, type) != kVariOffTypes.end();
}

void oscSteerVariTable(Vc2OscVoice& o, const OscEventTable& t) {
    o.setU8(Vc2OscVoice::kObject + kFirst, t.first);
    o.setU8(Vc2OscVoice::kObject + kStride, t.stride);
    o.setU32(Vc2OscVoice::kObject + kTable, kOscEventsAt);
}

void oscHoldVariEvents(Vc2OscVoice& o, const OscEventTable& t) {
    for (uint32_t at : kEvents) o.setU32(at, heldEvent(o.u32(at), t));
}

void oscHoldVariBlockEvents(std::span<uint32_t> block, const OscEventTable& t) {
    for (uint32_t at : kEvents) {
        uint32_t& word = block[(at - Vc2OscVoice::kOwn) / 4];
        word = heldEvent(word, t);
    }
}

void oscSteerLiteAsk(Vc2OscVoice& o, std::span<uint32_t> given) {
    constexpr uint32_t kHoldFrom = 0x3c, kAskSeek = 0x40;   // [FW] the settings' words, from their +0xb0
    constexpr uint8_t kHeld = 0x04;                          // [FW] the object's byte 0
    if (given[kAskSeek / 4] != 0) return;
    o.setU8(Vc2OscVoice::kObject, o.u8(Vc2OscVoice::kObject) | kHeld);
    given[kHoldFrom / 4] = 0;
}
#endif
