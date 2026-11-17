// xt_vc2osc's draws for the analog OSC, kind 3 (v242, v244; here from v246): its block words as its converter could
// give them and the object's words its sub oscillator's note-on reads.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace {
using AnalogWords = std::array<std::pair<uint32_t, uint32_t>, kOscAnalogWords>;

void setWord(AnalogWords& words, uint32_t at, float x) {
    const auto word = std::ranges::find(words, at, &std::pair<uint32_t, uint32_t>::first);
    word->second = std::bit_cast<uint32_t>(x);
}

// v246: with the sub oscillator on and an LA code, Fat (+0x108) within ±40 and its envelope's and LFO's depths
// (+0x10c, +0x114) within ±0.04, the bound outputs being at most 1000: its raw sum within ±120 octaves.  rom.4's powf
// gives ∞ from 128 on, and ∞ × a phase of 0 is a NaN that LA's end (0x893c3cd0) subtracts, where xtf::sub's NaN sign
// is FSUB's negated.  The other codes take Fat's ∞ through products alone, so they keep the moderate draws
void steerSubFat(AnalogWords& words, std::mt19937& rng) {
    constexpr float kFatMost = 40.0f, kDepthMost = 0.04f;
    setWord(words, 0x108, kFatMost * (2.0f * vc2Unit(rng) - 1.0f));
    for (uint32_t at : { 0x10cu, 0x114u }) setWord(words, at, kDepthMost * (2.0f * vc2Unit(rng) - 1.0f));
}
} // namespace

AnalogWords oscAnalogWords(std::mt19937& rng) {
    constexpr uint32_t kCodes = 11, kPast = 8, kToneKfs = 15;   // [FW] the codes 0…10; the Tone KF table's 15
    constexpr uint32_t kLaFirst = 9, kLaCodes = 2;              // [FW] LA-SQUARE's and LA-SAW's codes
    constexpr std::array<float, 6> kSubOn { 1.0f, 2.0f, -1.0f, 0.99f, 1e9f, -0.0f };
    uint32_t code = rng() % kPast == 0 ? kCodes + (uint32_t)rng() % kPast : (uint32_t)rng() % kCodes;
    // v246: the sub oscillator's switch drawn one time in two (one in four before), and with it on an LA code one time
    // in three, so that the shape's LA way is reached at 40 cases
    const float subOn = rng() % 2 == 0 ? kSubOn[rng() % kSubOn.size()] : 0.0f;
    if (std::trunc(subOn) != 0.0f && rng() % 3 == 0) code = kLaFirst + (uint32_t)rng() % kLaCodes;
    const float toneKf = (float)(rng() % kToneKfs) + 0.9f * vc2Unit(rng);
    // Impact as the converter gives it, Impact·4/40: its tenths 0…4, 1 (the default) one time in four; else moderate
    constexpr uint32_t kTenths = 41;
    const float tenths = (float)(rng() % kTenths) / 10.0f;
    const float impact = rng() % 4 == 0 ? 1.0f : rng() % 2 ? tenths : oscDrawFloat(rng, OscRange::moderate);
    AnalogWords words { { { 0x104, code }, { 0x138, std::bit_cast<uint32_t>(subOn) },
                          { 0x144, std::bit_cast<uint32_t>(toneKf) }, { 0x130, std::bit_cast<uint32_t>(impact) } } };
    constexpr std::array<uint32_t, 9> kModerate { 0x108, 0x10c, 0x114, 0x11c, 0x120, 0x124, 0x134, 0x140, 0x13c };
    for (size_t k = 0; k < kModerate.size(); ++k) {
        words[4 + k] = { kModerate[k], std::bit_cast<uint32_t>(oscDrawFloat(rng, OscRange::moderate)) };
    }
    if (code - kLaFirst < kLaCodes && std::trunc(subOn) != 0.0f) steerSubFat(words, rng);
    return words;
}

void oscAnalogObject(Vc2OscVoice& o, std::mt19937& rng) {
    // [FW] Fat's and the Pulse Width's targets, the phase, the second wave's Pulse Width target, the level's target,
    // RAMP's last segment, the two words nothing writes, LA's steepness targets, SQUARE's and JUNO's
    constexpr std::array<uint32_t, 12> kWords { 0x24c, 0x254, 0x260, 0x278, 0x28c, 0x294, 0x298, 0x2a0, 0x2b0, 0x2b8,
                                                0x2c0, 0x2c8 };
    for (uint32_t at : kWords) o.setF32(at, oscDrawFloat(rng, OscRange::moderate));
}

namespace {
// v242: a pitch within an octave of `centre` cents (the root's, or from v246 five octaves up): the pitch's word
// (+0x5c), its envelope's and vibrato's depths (+0x70, +0xfc) 0, its factor (+0x60) and the rate's ratio 1, the offset
// and the pitch ramp's value 0
void steerPitch(Vc2OscVoice& o, std::mt19937& rng, float centre) {
    constexpr uint32_t kPitch = 0x5c, kFactor = 0x60;   // [FW] the cents' and the ratio's words (Vc2OscTargets)
    constexpr float kOctave = 1200.0f;
    o.setF32(kPitch, centre + kOctave * (2.0f * vc2Unit(rng) - 1.0f));
    for (uint32_t at : { 0x70u, 0xfcu }) o.setF32(at, 0.0f);
    o.setF32(kFactor, 1.0f);
    o.tail.rateRatio = 1.0f;
    o.tail.offset = 0.0f;
    o.ramps[Vc2OscVoice::kPitchRamp].w.value = 0.0f;
}

// v246: a wave's code (2…7, SAW to JUNO) five octaves up (the ratio 16 to its most, 32), where its Hz nears the rate
// and the shape's hold passes ¼; the Pulse Width 0.1…0.4 alone, just above the holds' scale; Fat 0…3 octaves alone,
// the second wave's hold past the first's: where the ½ that picks a hold's side shows
void steerHighWave(Vc2OscVoice& o, std::mt19937& rng) {
    // [FW] the block's words: the code, Fat, the Pulse Width, their depths; SAW's code and the waves' count
    constexpr uint32_t kCode = 0x104, kFat = 0x108, kFatEnvelope = 0x10c, kFatLfo = 0x114, kPw = 0x11c;
    constexpr uint32_t kPwEnvelope = 0x120, kPwLfo = 0x124, kFirstWave = 2, kWaves = 6;
    constexpr float kFiveOctaves = 6000.0f, kPwLeast = 0.1f, kPwSpan = 0.3f, kFatMost = 3.0f;
    steerPitch(o, rng, kFiveOctaves);
    o.setU32(kCode, kFirstWave + (uint32_t)rng() % kWaves);
    o.setF32(kPw, kPwLeast + kPwSpan * vc2Unit(rng));
    o.setF32(kFat, kFatMost * vc2Unit(rng));
    for (uint32_t at : { kPwEnvelope, kPwLfo, kFatEnvelope, kFatLfo }) o.setF32(at, 0.0f);
}

// v246: with the sub oscillator on, one time in two a wave five octaves up (steerHighWave); and at HQ and NOISE one
// time in two (one in three until v248's draws thinned it) a negative phase and Impact, whose product the head keeps
// and the sub's note-on makes negative again, where their end holds it to 0 (the converter gives no negative Impact;
// rom.4 holds it all the same)
void steerSub(Vc2OscVoice& o, std::mt19937& rng) {
    // [FW] the block's words, the object's phase, and HQ-SQUARE's and NOISE's codes
    constexpr uint32_t kSubOn = 0x138, kCode = 0x104, kImpact = 0x130, kPhase = 0x260, kHqLast = 1, kNoise = 8;
    if (std::trunc(o.f32(kSubOn)) == 0.0f) return;
    if (rng() % 2 == 0) steerHighWave(o, rng);
    const uint32_t code = o.u32(kCode);
    if ((code <= kHqLast || code == kNoise) && rng() % 2 == 0) {
        o.setF32(kImpact, -vc2Unit(rng));
        o.setF32(kPhase, -vc2Unit(rng));
    }
}
} // namespace

// v242: kind 3's, the analog's, block words drawn; a pitch near the root half the time, then half of those SQUARE
// (code 3), whose steps it parts; the object's words moderate (oscAnalogObject), then the sub's (steerSub, v246); LA's
// codes (9, 10) half the time Impact below 1 and +0x298, which the phases' terms are held against, large, where the
// phase passes 1 (v244); NOISE's (code 8) Pulse Width one time in four a NaN, where its hold meets 0 (FTRC of a NaN: 0
// on both sides as a byte; a NaN alone in each product).  Not on the other codes: their sums take it through
// xtf::sub, whose NaN's sign is the interpreter's FSUB's negated
void oscSteerAnalog(OscSides& s, std::mt19937& rng, int v) {
    constexpr uint32_t kAnalog = 3, kPw = 0x11c, kCode = 0x104, kSquare = 3, kNoise = 8, kLaFirst = 9;   // [FW]
    constexpr uint32_t kImpact = 0x130, kUnwritten = 0x298;                                            // [FW]
    Vc2OscVoice& o = s.voices[(size_t)v];
    if (o.kind() != kAnalog) return;
    for (const auto& [at, bits] : oscAnalogWords(rng)) o.setU32(at, bits);
    if (rng() % 2) {
        steerPitch(o, rng, 0.0f);
        if (rng() % 2) o.setU32(kCode, kSquare);
    }
    oscAnalogObject(o, rng);
    steerSub(o, rng);
    if (o.u32(kCode) - kLaFirst < 2 && rng() % 2) {
        o.setF32(kImpact, 0.5f * vc2Unit(rng));
        o.setF32(kUnwritten, 2.0f + 100.0f * vc2Unit(rng));
    }
    if (o.u32(kCode) == kNoise && rng() % 4 == 0) o.setF32(kPw, NAN);
    oscWriteVoice(s, v);
}

void oscMakeAnalog(OscSides& s, std::mt19937& rng, int v) {
    constexpr uint32_t kAnalog = 3;   // [FW] the analog's kind
    s.voices[(size_t)v].setU32(Vc2OscVoice::kKind, kAnalog);
    oscSteerAnalog(s, rng, v);
}
#endif
