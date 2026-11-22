// LITE's words: see Vc2OscPcmLite.h (v395; from v396 the pitch and cents laws VariPhrase's words share).  The cents in
// XtFloat, as the SH-4 rounds; the integer sums in uint32_t, as its registers hold them.  Every address is rom.4's.
#include "Vc2OscPcmLite.h"
#include "Vc2InputReads.h"
#include "XtFloat.h"
#include <bit>
#include <cstring>

namespace {
using vc2in::s16;
using vc2in::s8;
using vc2in::u32;
using vc2in::u8;

// one-off: rom.4's offsets in the converter's input, declarations, not a job to template
// [FW] the input's words: the key, Pitch KF, the coarse and fine tunes, where the pitch10s go, the second key; the
// record's Start Offset, switches and mode; the wave's root key and its offset, no-wave flag, length, 48 bytes and last
// word
constexpr int kKeyAt = 3, kPitchKfAt = 0x0e, kCoarseAt = 0x88, kFineAt = 0x90, kSecondKeyAt = 0x9c;
constexpr int kPitch10At = 0x94, kKeyPitch10At = 0x98, kStartAt = 0x48, kSwitchE8At = 0x60, kSwitchE4At = 0x63;
constexpr int kModeAt = 0x64, kSwitchECAt = 0x67, kSwitchF0At = 0x68, kRootOffsetAt = 0xa5, kRootAt = 0xa8;
constexpr int kNoWaveAt = 0xb0, kLengthAt = 0xbc, kWaveBytesAt = 0xcc, kWaveWordAt = 0xfc;
constexpr int kCentre = 60, kPerKey = 1000;   // [FW] the keys' centre (LITE's); pitch10 a key
constexpr uint32_t kModeOn = 2;               // [FW] +0xb0 with Vari Sw
// [FW] the block's words by their index; the key's byte
constexpr size_t kKeyByte = 4, kCentsWord = 0x08 / 4, kKeyCentsWord = 0x68 / 4, kModeWord = 0xb0 / 4;
constexpr size_t kWaveBytesWord = 0xb4 / 4, kE4Word = 0xe4 / 4, kE8Word = 0xe8 / 4, kECWord = 0xec / 4;
constexpr size_t kF0Word = 0xf0 / 4, kF4Word = 0xf4 / 4, kStartWord = 0xf8 / 4, kLengthWord = 0xfc / 4;
constexpr size_t kStartAgainWord = 0x100 / 4, kWaveBytes = 48;

uint32_t oneIf(bool b) { return b ? 1u : 0u; }
} // namespace

void vc2OscPitchAbout(uint8_t* input, int centre, uint32_t* block) {
    const int key = u8(input, kKeyAt), coarse = s16(input, kCoarseAt), kf = s16(input, kPitchKfAt);
    const uint32_t fine = u32(input, kFineAt);
    const uint32_t p10 = vc2OscKeySum(key, coarse, kf, centre) + fine;
    const uint32_t keyP10 = vc2OscKeySum(u8(input, kSecondKeyAt), coarse, kf, centre) + fine;
    std::memcpy(input + kPitch10At, &p10, sizeof p10);
    std::memcpy(input + kKeyPitch10At, &keyP10, sizeof keyP10);
    reinterpret_cast<uint8_t*>(block)[kKeyByte] = vc2OscFollowedKey(key, coarse, kf, centre);
}

uint32_t vc2OscRootPitch10(const uint8_t* input) {
    const auto root = static_cast<int16_t>(u8(input, kRootAt) - s8(input, kRootOffsetAt));
    return static_cast<uint32_t>(root * kPerKey);
}

float vc2OscCentsFrom(uint32_t p10, uint32_t from, const Vc2OscCentsHold& c) {
    const float cents = xtf::div(xtf::flt(static_cast<int32_t>(p10 - from)), c.tenth);
    return vc2OscHeld(cents, c.least, c.most);
}

void vc2OscPcmLite(uint8_t* input, uint32_t tripMoving, uint32_t* block) {
    const Vc2OscCentsHold& hold = vc2OscConvertTables().lite;
    vc2OscPitchAbout(input, kCentre, block);
    const uint32_t root = vc2OscRootPitch10(input);
    block[kCentsWord] = std::bit_cast<uint32_t>(vc2OscCentsFrom(u32(input, kPitch10At), root, hold));
    block[kKeyCentsWord] = std::bit_cast<uint32_t>(vc2OscCentsFrom(u32(input, kKeyPitch10At), root, hold));
    block[kModeWord] = u8(input, kModeAt) != 0 ? kModeOn : 0;
    const uint32_t start = u32(input, kStartAt), length = u32(input, kLengthAt);
    block[kStartWord] = start >= length ? length - 1 : start;
    block[kLengthWord] = length;
    block[kStartAgainWord] = block[kStartWord];
    block[kE4Word] = oneIf(u8(input, kSwitchE4At) != 0 || u8(input, kNoWaveAt) == 0);
    block[kECWord] = oneIf(tripMoving != 0 && u8(input, kSwitchECAt) != 0);
    block[kE8Word] = oneIf(u8(input, kSwitchE8At) != 0);
    block[kF0Word] = oneIf(u8(input, kSwitchF0At) != 0);
    std::memcpy(block + kWaveBytesWord, input + kWaveBytesAt, kWaveBytes);
    block[kF4Word] = oneIf(u32(input, kWaveWordAt) != 0);
}
