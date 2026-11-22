// LITE's words: see Vc2OscPcmLite.h (v395).  The cents in XtFloat, as the SH-4 rounds; the integer sums in uint32_t, as
// its registers hold them.  Every address is rom.4's.
#include "Vc2OscPcmLite.h"
#include "Vc2InputReads.h"
#include "Vc2OscConvertTables.h"
#include "XtFloat.h"
#include <bit>
#include <cstring>

namespace {
using vc2in::s32;
using vc2in::s8;
using vc2in::u8;

constexpr size_t kInputBytes = 0x108;   // [FW] an OSC converter's input
// one-off: rom.4's offsets in the converter's input, declarations, not a job to template
// [FW] the input's words: the pitch10s v251's head writes; the record's start sample, switches and mode; the wave's
// root key and its offset, no-wave flag, length, 48 bytes and last word
constexpr int kPitch10At = 0x94, kKeyPitch10At = 0x98, kStartAt = 0x48, kSwitchE8At = 0x60, kSwitchE4At = 0x63;
constexpr int kModeAt = 0x64, kSwitchECAt = 0x67, kSwitchF0At = 0x68, kRootOffsetAt = 0xa5, kRootAt = 0xa8;
constexpr int kNoWaveAt = 0xb0, kLengthAt = 0xbc, kWaveBytesAt = 0xcc, kWaveWordAt = 0xfc;
constexpr int kPerKey = 1000;     // [FW] pitch10 a key
constexpr uint32_t kModeOn = 2;   // [FW] +0xb0 with the record's mode byte
// [FW] the block's words by their index; the key's byte
constexpr size_t kKeyByte = 4, kCentsWord = 0x08 / 4, kKeyCentsWord = 0x68 / 4, kModeWord = 0xb0 / 4;
constexpr size_t kWaveBytesWord = 0xb4 / 4, kE4Word = 0xe4 / 4, kE8Word = 0xe8 / 4, kECWord = 0xec / 4;
constexpr size_t kF0Word = 0xf0 / 4, kF4Word = 0xf4 / 4, kStartWord = 0xf8 / 4, kLengthWord = 0xfc / 4;
constexpr size_t kStartAgainWord = 0x100 / 4, kWaveBytes = 48;

uint32_t inputWord(const uint8_t* input, int at) { return static_cast<uint32_t>(s32(input, at)); }

uint32_t oneIf(bool b) { return b ? 1u : 0u; }

// A pitch10 against the wave's root key: ÷ 10, held to ±6000, the upper bound first
float rootCents(uint32_t p10, const uint8_t* input) {
    const Vc2OscConvertTables& t = vc2OscConvertTables();
    const auto root = static_cast<int16_t>(u8(input, kRootAt) - s8(input, kRootOffsetAt));
    const uint32_t from = p10 - static_cast<uint32_t>(root * kPerKey);
    const float cents = xtf::div(xtf::flt(static_cast<int32_t>(from)), t.liteTenth);
    return vc2OscHeld(cents, t.liteCentsLeast, t.liteCentsMost);
}

// The pitch10s into the input and the key into the block, as v251's head makes them, from it run on a copy
void litePitch(uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared, uint32_t* block) {
    std::array<uint8_t, kInputBytes> copy;
    std::memcpy(copy.data(), input, copy.size());
    Vc2OscBlockHead made {};
    vc2OscBlockConvert(copy.data(), kVc2OscVocoderRow, in, shared, made);
    std::memcpy(input + kPitch10At, copy.data() + kPitch10At, sizeof(uint32_t));
    std::memcpy(input + kKeyPitch10At, copy.data() + kKeyPitch10At, sizeof(uint32_t));
    reinterpret_cast<uint8_t*>(block)[kKeyByte] = made.key;
}
} // namespace

void vc2OscPcmLite(uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared, uint32_t tripMoving,
                   uint32_t* block) {
    litePitch(input, in, shared, block);
    block[kCentsWord] = std::bit_cast<uint32_t>(rootCents(inputWord(input, kPitch10At), input));
    block[kKeyCentsWord] = std::bit_cast<uint32_t>(rootCents(inputWord(input, kKeyPitch10At), input));
    block[kModeWord] = u8(input, kModeAt) != 0 ? kModeOn : 0;
    const uint32_t start = inputWord(input, kStartAt), length = inputWord(input, kLengthAt);
    block[kStartWord] = start >= length ? length - 1 : start;
    block[kLengthWord] = length;
    block[kStartAgainWord] = block[kStartWord];
    block[kE4Word] = oneIf(u8(input, kSwitchE4At) != 0 || u8(input, kNoWaveAt) == 0);
    block[kECWord] = oneIf(tripMoving != 0 && u8(input, kSwitchECAt) != 0);
    block[kE8Word] = oneIf(u8(input, kSwitchE8At) != 0);
    block[kF0Word] = oneIf(u8(input, kSwitchF0At) != 0);
    std::memcpy(block + kWaveBytesWord, input + kWaveBytesAt, kWaveBytes);
    block[kF4Word] = oneIf(inputWord(input, kWaveWordAt) != 0);
}
