// EXTIN's OSC converter: see Vc2OscExtinConvert.h (v393).  Each routine's arithmetic in its own order, in XtFloat; the
// integer sums as the SH-4's 32-bit registers hold them.  Every address is rom.4's.
#include "Vc2OscExtinConvert.h"
#include "Vc2InputReads.h"
#include "Vc2OscConvertTables.h"
#include "XtFloat.h"
#include <algorithm>
#include <bit>
#include <cstring>

namespace {
using vc2in::s16;
using vc2in::s8;
using vc2in::u8;
using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;

// [FW] the input's words, by its offset: the header's kind, OSC and velocity; the record's Wave Gain, the formant's
// depth and keyfollow, its LFO's and envelope's depths, the envelope's velocity curve and sens, the formant's row
constexpr int kKindAt = 1, kOscAt = 2, kVelocityAt = 4, kGainAt = 0x0a, kFormantAt = 0x56, kFollowAt = 0x58;
constexpr int kFormantLfoAt = 0x5a, kFormantEnvAt = 0x5b, kCurveAt = 0x5c, kSensAt = 0x5e, kRowAt = 0x66;
constexpr uint32_t kOwnKind = 6, kHeldKind = 7;          // [FW] the kinds the converter writes
constexpr int32_t kEnvWords[2] = { 3, 7 };               // [FW] +0x30, by OSC
constexpr int kOscFormant = 2, kLfoFormant = 3;          // the groups' Formant sums
constexpr int kDepthMost = 63, kLevelIndex = 40;         // [FW] a depth's hold; the level curve's word for +0x18
constexpr int kLastRow = 10;                             // [FW] 0x8939c490's hold of its row
constexpr float kTwo = 2.0f;                             // fldi1, added to itself
// [FW] the block's words by their index: the key byte, the formant, the level's word, the envelope's depth and word,
// the LFO's depth, a 0, and the systems' flag at +0xb0
constexpr size_t kKeyByte = 4, kFormantWord = 0x14 / 4, kLevelWord = 0x18 / 4, kEnvDepthWord = 0x2c / 4;
constexpr size_t kEnvWord = 0x30 / 4, kLfoDepthWord = 0x4c / 4, kZeroWord = 0x50 / 4, kSystemWord = 0xb0 / 4;

const Vc2OscConvertTables& extinTables() { return vc2OscConvertTables(); }

uint32_t blockWord(float f) { return std::bit_cast<uint32_t>(f); }

// The key's distance from the row's key whose span holds it, and the offset that span adds; rom.4's third span takes
// its distance from the second's key, as the routine does
struct FormantSpan {
    int32_t from, offset;
};
FormantSpan formantSpanOf(const std::array<int32_t, 8>& r, int32_t key) {
    if (key < r[0]) return { r[0], r[1] };
    if (key < r[2]) return { r[2], r[3] };
    if (key < r[4]) return { r[2], r[5] };
    return { r[4], r[7] };
}
} // namespace

float vc2OscExtinFormant(int16_t depth, int16_t sum, uint8_t row, int16_t follow, int16_t key) {
    const Vc2OscConvertTables& t = extinTables();
    const FormantSpan s = formantSpanOf(t.formantRows[std::min<int>(row, kLastRow)], key);
    const auto distance = static_cast<uint32_t>(key - s.from);
    const auto offset = static_cast<int16_t>(depth + s.offset);
    const float keyed = div(mul(flt(static_cast<int32_t>(static_cast<uint32_t>(follow) * distance)), t.formantStep),
                            t.formantCents);
    return vc2OscFormantHeld(offset, sum, keyed, t.sumLeast);
}

float vc2OscFormantHeld(int16_t depth, int16_t sum, float keyed, float least) {
    const Vc2OscConvertTables& t = extinTables();
    const float held = vc2OscHeld(add(div(mul(flt(depth), t.formantMost), t.formantDepth), flt(sum)), least,
                                  t.formantMost);
    const float scaled = div(mul(div(held, t.formantMost), t.formantDepth), t.formantDepth);
    return vc2OscHeld(add(scaled, keyed), -1.0f, 1.0f);
}

void vc2OscExtinConvert(uint8_t* input, const Vc2OscConvertIn& in, const Vc2OscExtinIn& x, const Vc2ToneShared& shared,
                        uint32_t* block) {
    const Vc2OscConvertTables& t = extinTables();
    Vc2OscBlockHead head;
    std::memcpy(&head, block, sizeof head);
    vc2OscBlockConvert(input, kVc2OscVocoderRow, in, shared, head);
    head.gain = shared.pow(kTwo, div(flt(s8(input, kGainAt)), t.extinGainScale));
    std::memcpy(block, &head, sizeof head);
    block[kSystemWord] = x.system == 0 ? 1 : 0;
    if (x.header != 0) {
        if (u8(input, kKindAt) == 0) block[0] = kHeldKind;
        return;
    }
    block[0] = kOwnKind;
    const auto curve = static_cast<uint16_t>(shared.velocity(u8(input, kVelocityAt), s16(input, kSensAt),
                                                             u8(input, kCurveAt)));
    const float velocity = div(flt(curve), t.extinVelocityScale);
    const auto key = static_cast<int16_t>(static_cast<int8_t>(reinterpret_cast<const uint8_t*>(block)[kKeyByte]));
    const auto formant = static_cast<int16_t>(s8(input, kFormantAt));
    const auto follow = static_cast<int16_t>(s16(input, kFollowAt));
    block[kFormantWord] =
        blockWord(vc2OscExtinFormant(formant, in.sums.osc[kOscFormant], u8(input, kRowAt), follow, key));
    block[kEnvWord] = static_cast<uint32_t>(kEnvWords[u8(input, kOscAt) == 0 ? 0 : 1]);
    block[kEnvDepthWord] = blockWord(mul(div(flt(s8(input, kFormantEnvAt)), t.extinDepthTop), velocity));
    block[kZeroWord] = 0;
    const int depth = vc2OscHeld(in.sums.lfo[kLfoFormant] + s8(input, kFormantLfoAt), -kDepthMost, kDepthMost);
    block[kLfoDepthWord] = blockWord(div(flt(depth), t.extinDepthTop));
    block[kLevelWord] = blockWord(div(flt(vc2OscLevelWord(t, kLevelIndex)), t.extinVelocityScale));
}
