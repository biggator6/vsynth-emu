// The VC-2's OSC voice's settings and getters: see Vc2OscVoice.h (v230, v232).  Every address is rom.4's.
#include "Vc2OscTables.h"
#include "Vc2OscTargets.h"
#include "Vc2OscVoiceKinds.h"
#include "XtFloat.h"

using xtf::mul;
using xtf::sub;

namespace {
// [FW] 0x893ae780's copy, in its order: each of the voice's words from the settings' word 0x54 below it (the block's
// layout), but +0x70, which takes the settings' +0x1c × 0.2
constexpr std::array<uint32_t, 19> kCopied { 0xbc, 0xc4, 0xcc, 0x5c, 0x60, 0x64, 0x68, 0x6c, 0x78, 0x80, 0x90, 0x98,
                                              0xa0, 0x88, 0xa8, 0xb0, 0xfc, 0x100, 0xb4 };
constexpr uint32_t kScaled = 0x70, kScaledFrom = 0x1c, kByte = 0x58, kSettingsByte = 4, kRampFr4 = 0xbc;
constexpr uint32_t kRampFr5 = 0x5c, kVibratoFrom = 0x80, kKindFrom = 0xb0, kCarrierBytes = 88;
// [FW] the getters' words: the formant (+0x68) and the time (+0x64) of kind 2 (rom.2's block +0x14, +0x10), the
// object's level (+0x1e0), its +0x168, and its positions by the wave's type (+0x24c, +0x194, +0x270)
constexpr uint32_t kFormant = 0x68, kTime = 0x64;
constexpr uint32_t kLevel = 0x1e0, kHalf = 0x168, kPositionLite = 0x24c, kPosition = 0x194, kPositionFloat = 0x270;
constexpr int kPositionShift = 4;
constexpr float kTwo31 = 2147483648.0f;   // [FW] 0x893aedc4…0x893aedd0: mov #79, shll8, shll16, as a float

float settingsWord(const uint32_t* s, uint32_t at) { return std::bit_cast<float>(s[at / 4]); }
} // namespace

void Vc2OscVoice::settings(const uint32_t* s, Vc2OscParts& parts, const Vc2ToneShared& shared) {
    for (uint32_t at : kCopied) setU32(at, s[(at - kOwn) / 4]);
    setF32(kScaled, mul(settingsWord(s, kScaledFrom), vc2OscTables().settingsScale));
    const auto* bytes = reinterpret_cast<const uint8_t*>(s);
    if (bytes[kSettingsByte] != u8(kByte)) {
        setU8(kByte, bytes[kSettingsByte]);
        if (kind() >= (uint32_t)kVc2OscKinds) return;
        // [FW] the pitch's ramp again (the pool word 0x893ae90c holds its restart, 0x893b0f20)
        if (vc2OscKindRow(kind()).ramp) ramps[kPitchRamp].restart(f32(kRampFr4), f32(kRampFr5), shared);
    }
    Vc2VibratoBlock rates;
    std::memcpy(&rates, s + kVibratoFrom / 4, sizeof rates);
    vibrato.settings(rates);
    const Vc2OscKindRow& row = vc2OscKindRow(kind());
    if (row.settingsWay == Vc2OscSettings::analog) settingsAnalog(s + kKindFrom / 4);   // v248
    else if (row.settingsWay == Vc2OscSettings::lite) settingsLite(s + kKindFrom / 4, parts);   // v324
    else if (row.settings != 0) parts.kindSettings(row.settings, s + kKindFrom / 4);
    if (!row.carrier) return;
    std::memcpy(own.data() + (kWave - kOwn), s + kKindFrom / 4, kCarrierBytes);
    Vc2CarrierParams given;
    std::memcpy(&given, s + kKindFrom / 4, sizeof given);
    carrier.settings(&given);   // 0x893ec450, on the settings' words themselves
}

float Vc2OscVoice::formant() const { return kind() == 2 && u32(kWave) == 1 ? f32(kFormant) : 0.0f; }

float Vc2OscVoice::time() const { return kind() == 2 ? f32(kTime) : 0.0f; }

float Vc2OscVoice::pitch(Vc2OscParts& parts) const {
    const Vc2OscPitch way = vc2OscKindRow(kind()).pitch;
    if (way == Vc2OscPitch::none) return 0.0f;
    const float bounded = vc2OscBounded(vc2OscPitchSum(*this, parts), vc2OscTables().getterCents);
    return way == Vc2OscPitch::tuned ? sub(bounded, tail.detune) : bounded;   // [FW] 0x893aec88: 3, 5, 6 return at once
}

// [FW] 0x893aecc0 and 0x893aecf0 end in the targets 0x893afeb0 and 0x893afdf0 (v232: their ports)
float Vc2OscVoice::kindFormant(Vc2OscParts& parts, const Vc2ToneShared& shared) const {
    return kind() == 2 && u32(kWave) == 1 ? Vc2OscTargets(*this, parts, shared).formant() : 0.0f;
}

float Vc2OscVoice::kindWalk(Vc2OscParts& parts, const Vc2ToneShared& shared) const {
    return kind() == 2 ? Vc2OscTargets(*this, parts, shared).walk() : 0.0f;
}

float Vc2OscVoice::level() const { return kind() != 0 ? f32(kLevel) : 0.0f; }

int32_t Vc2OscVoice::word168() const { return kind() == 2 ? (int16_t)u32(kHalf) : 0; }

uint32_t Vc2OscVoice::position() const {
    const int32_t samples = (int32_t)u32(kPosition) >> kPositionShift;
    if (kind() == 2) return (uint32_t)samples;
    if (kind() != 1) return 0;
    const uint32_t type = u32(kWave);
    if (type == 0) return u32(kPositionLite);
    if (type == 1) return (uint32_t)samples;
    if (type != 2) return 0;
    const float x = f32(kPositionFloat);
    return (uint32_t)xtf::ftrc(kTwo31 > x ? x : sub(x, kTwo31 + kTwo31));
}
