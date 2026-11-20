// xt_vc2osc's draws for the analog OSC, kind 3 (v242, v244; here from v246): its steers of a voice's words, a pitch
// near the root, the sub oscillator's and LA's.  Its block words and its object's words are Vc2OscDraws.cpp's from v319,
// shared with xt_vc2tone.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2OscSides.h"
#include <algorithm>
#include <bit>
#include <cmath>

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
    // v321: one time in three HQ-SQUARE, HQ-SAW or NOISE: a negative phase at their end (the chain of a kind-3 note,
    // the sub on, one of these codes and the phase) came up too rarely at 40 cases once v321's draws moved the run
    if (rng() % 3 == 0) o.setU32(kCode, rng() % 3 == 0 ? kNoise : (uint32_t)rng() % (kHqLast + 1));
    const uint32_t code = o.u32(kCode);
    if ((code <= kHqLast || code == kNoise) && rng() % 2 == 0) {
        o.setF32(kImpact, -vc2Unit(rng));
        o.setF32(kPhase, -vc2Unit(rng));
    }
}

// v267: one time in two the sub oscillator off, whose note-on otherwise takes the place of LA's, and Fat alone and well
// below 0, which makes the LA phases' Fat term large: the phase then passes 1, where the note-on sets it to 0.  The
// moderate draws reached that about once in 40 cases, too rarely to rely on
void steerLaPhase(Vc2OscVoice& o, std::mt19937& rng) {
    constexpr uint32_t kSubOn = 0x138, kFat = 0x108, kFatEnvelope = 0x10c, kFatLfo = 0x114;   // [FW] the block's words
    if (rng() % 2) return;
    o.setF32(kSubOn, 0.0f);
    o.setF32(kFat, -4.0f - 8.0f * vc2Unit(rng));
    for (uint32_t at : { kFatEnvelope, kFatLfo }) o.setF32(at, 0.0f);
}

// v267: one time in six an LA code with the sub oscillator off, so that LA's own note-on comes up in every run; with the
// sub on it would need steerSubFat's Fat, which these words weren't drawn with
void steerLaCode(Vc2OscVoice& o, std::mt19937& rng) {
    constexpr uint32_t kCode = 0x104, kSubOn = 0x138, kLaFirst = 9, kLaCodes = 2;   // [FW]
    if (rng() % 6 != 0) return;
    o.setU32(kCode, kLaFirst + (uint32_t)rng() % kLaCodes);
    o.setF32(kSubOn, 0.0f);
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
    steerLaCode(o, rng);
    if (o.u32(kCode) - kLaFirst < 2 && rng() % 2) {
        o.setF32(kImpact, 0.5f * vc2Unit(rng));
        o.setF32(kUnwritten, 2.0f + 100.0f * vc2Unit(rng));
        steerLaPhase(o, rng);
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
