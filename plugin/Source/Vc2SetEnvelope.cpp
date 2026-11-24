// The VC-2 part's conversion sets' envelopes: see Vc2SetEnvelope.h (v282).  Each routine's reads, calls and writes in
// rom.4's order; the floats through XtFloat, as the SH-4 rounds.
#include "Vc2SetEnvelope.h"
#include "Vc2LibMath.h"
#include "Vc2SetChildHead.h"
#include "Vc2SetEnvelopeTables.h"
#include "XtFloat.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <ranges>

namespace {
constexpr uint32_t kUnit = 0x10000;               // [FW] a factor of 1, 16.16
constexpr uint32_t kHead = 0x124f80;              // [FW] the block's +0x0c and +0x10
constexpr int32_t kLongest = 12000;               // [FW] the time curve's most
constexpr int32_t kNoteFrom = 128, kTop = 127;    // [FW] a value from 128 on is a note value; a sum's most
constexpr int32_t kCentreKey = 60, kCentreVelocity = 64, kTicksTimes = 24;
constexpr int32_t kDepthScale = 256000;           // [FW] a depth's, before the division by 6400
constexpr int32_t kMagic = 0x51eb851f;            // [FW] rom.4's reciprocal of 6400, by its shift
constexpr int kMagicShift = 11, kVelocityShift = 8;
constexpr uint32_t kLastEnvelope = 12, kTypeEnvelope = 3, kTypeFrom = 6;   // [FW] the switches on the index
constexpr uint32_t kFadeType = 2;
constexpr float kOne = 1.0f, kZero = 0.0f, kTwo = 2.0f;
constexpr float kTopFactor = 2147483648.0f;       // [FW] the factor's unsigned conversion's 2^31

// [FW] a Matrix group's reader for an envelope from `first` on, and its sums' offset for that one; each envelope's
// three sums (attack, decay, release) are 2 bytes apart, the next envelope's 6 on
struct Reader {
    uint32_t routine;
    uint32_t first;
    uint32_t base;
};
constexpr Reader kReaders[] = {
    { 0x893a2550, 0, 0 }, { 0x893a2750, 4, 0 }, { 0x893a2960, 8, 4 }, { 0x893a2aa0, 10, 4 }, { 0x893a2bf0, 12, 0 },
};

const Vc2SetEnvelopeTables& envelopeTables() { return vc2SetEnvelopeTables(); }

// 0x89399ff0, 0x8939a130, 0x8939a270 by `stage` (0 attack, 1 decay, 2 release): the value plus the Matrix's sum for
// the envelope, held to 0…127
uint32_t matrixTime(uint32_t value, uint32_t matrix, uint32_t index, uint32_t stage, Vc2SetReads& reads) {
    int32_t sum = 0;
    if (index <= kLastEnvelope) {
        const auto later = [&](const Reader& x) { return x.first <= index; };
        const Reader& r = *std::ranges::find_if(kReaders | std::views::reverse, later);
        sum = reads.matrixSum(r.routine, matrix, r.base + 6 * (index - r.first) + 2 * stage);
    }
    return static_cast<uint32_t>(std::clamp(static_cast<int32_t>(value & 0xff) + sum, 0, kTop));
}


// 0x8939aa40: the time of a value less the velocity's part by its depth (none at or below 0), then by the key's factor
// (none at 1; held to 12000 past it; at least 1)
uint32_t curveTime(uint32_t value, uint32_t velocity, int8_t sens, uint32_t factor) {
    const int32_t time = static_cast<int16_t>(envelopeTables().times[value]);
    if (time == 0) return 0;
    uint32_t held = static_cast<uint32_t>(time);
    if (sens != 0) {
        const int32_t depth = vc2ByMagic(sens * kDepthScale, kMagic, kMagicShift);
        const int32_t towards = static_cast<int32_t>(velocity & 0xff) - kCentreVelocity;
        const int32_t less = static_cast<int32_t>(static_cast<uint16_t>(time)) - ((depth * towards) >> kVelocityShift);
        if (less <= 0) return 0;
        held = static_cast<uint32_t>(std::min(less, kLongest));
    }
    if (factor == kUnit) return held & 0xffff;
    uint32_t out = ((factor & 0xffff) * (held & 0xffff)) >> 16;
    if (factor > kUnit) out = std::min(out + (factor >> 16) * (held & 0xffff), static_cast<uint32_t>(kLongest));
    return (out & 0xffff) == 0 ? 1 : out & 0xffff;
}

// 0x8939ab10: the time of a value, a quarter
uint32_t quarterTime(uint32_t value) { return static_cast<uint32_t>(envelopeTables().times[value] >> 2); }

// A note value's time: its ticks over 24 × the tempo, by `scale`
float noteTime(uint32_t value, uint16_t tempo, float scale) {
    const float ticks = xtf::flt(envelopeTables().notes[value - kNoteFrom]);
    return xtf::mul(xtf::div(ticks, xtf::flt(static_cast<int16_t>(tempo) * kTicksTimes)), scale);
}

// 0x8939a920: 2 to the key's distance from 60 by the key follow over 1200, as 16.16, an unsigned conversion
uint32_t keyFactor(uint8_t key, int16_t keyfollow) {
    const uint32_t against = static_cast<uint32_t>(-static_cast<int32_t>(keyfollow));
    const uint32_t steps = static_cast<uint32_t>(key - kCentreKey) * against;
    const float x = xtf::div(xtf::flt(static_cast<int32_t>(steps)), envelopeTables().cents);
    float y = xtf::mul(vc2LibPow(kTwo, x), envelopeTables().unit);
    if (!(kTopFactor > y)) y = xtf::sub(y, xtf::add(kTopFactor, kTopFactor));
    return static_cast<uint32_t>(xtf::ftrc(y));
}

float releaseTime(const Vc2SetEnvelopeWords& w, float scale, Vc2SetReads& reads) {
    if (w.release >= kNoteFrom) return noteTime(w.release, w.tempo, scale);
    const uint32_t value = matrixTime(w.release, w.matrix, w.index, 2, reads);
    return xtf::flt(static_cast<int32_t>(curveTime(value, w.releaseVelocity, w.releaseSens, w.factor)));
}

float attackTime(const Vc2SetEnvelopeWords& w, Vc2SetReads& reads) {
    if (w.attack >= kNoteFrom) return noteTime(w.attack, w.tempo, envelopeTables().noteScale);
    const uint32_t value = matrixTime(w.attack, w.matrix, w.index, 0, reads);
    return xtf::flt(static_cast<int32_t>(curveTime(value, w.velocity, w.attackSens, kUnit)));
}

float decayTime(const Vc2SetEnvelopeWords& w, uint32_t type, Vc2SetReads& reads) {
    if (w.decay >= kNoteFrom) return noteTime(w.decay, w.tempo, envelopeTables().noteScale);
    const uint32_t value = matrixTime(w.decay, w.matrix, w.index, 1, reads);
    const uint32_t time = type == 0 ? quarterTime(value) : curveTime(value, w.velocity, w.decaySens, w.factor);
    return xtf::flt(static_cast<int32_t>(time));
}
} // namespace

void Vc2SetEnvelopes::blockOf(Vc2SetEnvelopeWords& w, uint32_t* block, Vc2SetReads& reads) {
    const uint32_t type = w.index == kTypeEnvelope || w.index > kTypeFrom ? kFadeType : 0;
    block[0] = type;
    block[1] = floatWord(kOne);
    block[2] = 0;
    block[3] = kHead;
    block[4] = kHead;
    w.factor = keyFactor(w.key, w.keyfollow);
    const bool on = w.on != 0;
    block[5] = floatWord(on ? attackTime(w, reads) : kZero);
    block[6] = floatWord(kZero);
    block[7] = floatWord(on ? decayTime(w, type, reads) : kZero);
    block[8] = floatWord(on ? releaseTime(w, envelopeTables().noteScale, reads) : kZero);
    block[9] = floatWord(kZero);
    block[10] = floatWord(kOne);
    block[11] = floatWord(kOne);
    const float sustain = xtf::div(xtf::flt(envelopeTables().levels[w.sustain]), envelopeTables().levelScale);
    block[12] = floatWord(on ? sustain : kOne);
    block[13] = floatWord(kZero);
}

void Vc2SetEnvelopes::construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads) {
    Vc2SetEnvelopeWords& w = at(c.object);
    vc2ChildMade(vc2ChildBytes(w), c, reads);
    w.matrix = c.matrix;
    w.releaseVelocity = kCentreVelocity;
    vc2ChildPart(vc2ChildBytes(w), &Vc2SetReads::envelopeOf, c.call, reads);
    w.tempo = vc2SetTempo(reads.clock());
    blockOf(w, block, reads);
}

void Vc2SetEnvelopes::release(uint32_t envelope, uint32_t velocity, uint32_t* block, Vc2SetReads& reads) {
    Vc2SetEnvelopeWords& w = at(envelope);
    w.releaseVelocity = static_cast<uint8_t>(velocity);
    block[8] = floatWord(w.on != 0 ? releaseTime(w, envelopeTables().releaseScale, reads) : kZero);
}
