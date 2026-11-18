// The analog converter's second part: see Vc2OscAnalogConvert.h (v256).  Each routine's arithmetic in its own order, in
// XtFloat.  Every address is rom.4's.
#include "Vc2OscAnalogConvert.h"
#include "Vc2InputReads.h"
#include "XtFloat.h"
#include <iterator>

namespace {
using vc2in::s16;
using vc2in::s8;
using vc2in::u8;
using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;
using xtf::sub;

// [FW] the input's words, by its offset: the header's OSC and key; the record's waveform, Impact, Pulse Width with its
// KF, LFO and envelope depths and the envelope's velocity curve and sens, Fat the same, Detune, Sub Octave and Sub Level
constexpr int kOscAt = 2, kKeyAt = 3, kWaveAt = 0x28, kImpactAt = 0x29, kWidthAt = 0x2a, kWidthKfAt = 0x2c;
constexpr int kWidthLfoAt = 0x2e, kWidthEnvAt = 0x2f, kWidthCurveAt = 0x30, kWidthSensAt = 0x32, kFatAt = 0x34;
constexpr int kFatKfAt = 0x36, kFatLfoAt = 0x38, kFatEnvAt = 0x39, kFatCurveAt = 0x3a, kFatSensAt = 0x3c;
constexpr int kDetuneAt = 0x3e, kSubOctaveAt = 0x3f, kSubLevelAt = 0x40;
constexpr int kCentre = 60, kDepthMost = 63, kWidthWrap = 126;      // [FW] immediates, and 0x8939c6a0's magic division
constexpr int kOscWidth = 1, kOscFat = 2, kLfoWidth = 2, kLfoFat = 3;   // the groups' sums the part reads
constexpr int32_t kFatEnvWords[2] = { 3, 7 }, kWidthEnvWords[2] = { 2, 6 };   // [FW] by OSC
constexpr int32_t kCodes[] = { 2, 3, 4, 6, 5, 7, 0, 1, 8, 10, 9 };   // [FW] 0x8939d3b4's cases, by the waveform's byte
constexpr int kSaw = 0, kSquare = 1, kLaSquare = 10;                 // [FW] the waveforms the words below tell apart

// What Fat's words take either way: the key from the centre, the envelope's velocity factor, the LFO's depth with its sum
struct Vc2OscFatTerms {
    int k;
    float velocity;
    int lfo;
};

// The waveform's routine: past the switch's ten cases, and at 0, code 2
int32_t codeOf(int wave) { return wave < (int)std::size(kCodes) ? kCodes[wave] : kCodes[0]; }

// 0x8939cae0: SAW's and SQUARE's Slope ÷ 100 with Alias Cancel on, the other waveforms' 0
float aliasSlope(int wave, const Vc2OscAnalogSystem& s, const Vc2OscConvertTables& t) {
    if (wave > kSquare || s.aliasCancel == 0) return 0.0f;
    return div(flt(wave == kSaw ? s.sawSlope : s.squareSlope), t.slopeScale);
}

// 0x8939c5e0: the Pulse Width, the knob with the Matrix's sum within ±63, M's two bytes and the keyfollow, wrapped by
// its integer part into 0…126, ÷ 126
float widthOf(int width, int sum, int kf, int k, const Vc2OscConvertIn& in, const Vc2OscConvertTables& t) {
    const float x = add(div(mul(flt(sum), t.depthMost), t.sumScale), flt(width));
    const float held = vc2OscHeld(x, t.depthLeast, t.depthMost);
    const float halved = div(flt(in.widthBeam[0]), 2.0f);
    const float beam = div(mul(sub(flt(in.widthBeam[1]), halved), t.depthMost), t.beamScale);
    const float follow = div(mul(flt(kf * k), t.widthSteps), t.perOctave);
    const float at = add(add(beam, add(follow, held)), t.depthMost);
    int whole = xtf::ftrc(at);
    const float part = sub(at, flt(whole));
    if (whole < 0) {
        whole = whole % kWidthWrap + kWidthWrap;
    } else if (whole > kWidthWrap) {
        whole %= kWidthWrap;
    }
    return div(add(flt(whole), part), t.widthSteps);
}

// The Detune curve (inline in the converter, and 0x8939c6e0's): a depth in cents by its lines, the sign put back
float detuneCents(float x, const Vc2OscConvertTables& t) {
    const bool below = 0.0f > x;
    return mul(vc2OscOnLines(below ? -x : x, t.detune), below ? -1.0f : 1.0f);
}

// 0x8939c6e0: the sub oscillator's pitch in octaves, Detune with the Matrix's sum through the curve, the keyfollow
// added, within ±1
float detunedOctaves(int detune, int sum, int kf, int k, const Vc2OscConvertTables& t) {
    const float follow = div(mul(flt(kf * k), t.detuneKeyScale), t.perOctave);
    const float x = add(div(mul(flt(detune), t.sumScale), t.depthMost), flt(sum));
    const float held = vc2OscHeld(x, t.sumLeast, t.sumScale);
    const float depth = mul(div(held, t.sumScale), t.depthMost);
    return vc2OscHeld(add(div(detuneCents(depth, t), t.perOctave), follow), -1.0f, 1.0f);
}

// [FW] 0x8939d622's switch: Sub Octave's −2 and −1 octaves, its other values 0
float subOctaves(int octave, const Vc2OscConvertTables& t) {
    if (octave == 1) return t.subLowest;
    return octave == 2 ? -1.0f : 0.0f;
}

// With the sub oscillator on: Fat's words hold the second wave's pitch and its depths, through the Detune curve
void subWords(const uint8_t* input, const Vc2OscConvertIn& in, const Vc2OscFatTerms& f, Vc2OscAnalogWords& w,
              const Vc2OscConvertTables& t) {
    w.subOn = 1.0f;
    const float octaves = detunedOctaves(s8(input, kDetuneAt), in.sums.osc[kOscFat], s16(input, kFatKfAt), f.k, t);
    w.fat = add(octaves, subOctaves(u8(input, kSubOctaveAt), t));
    w.fatEnv = mul(div(detuneCents(flt(s8(input, kFatEnvAt)), t), t.perOctave), f.velocity);
    w.fatLfo = div(detuneCents(flt(f.lfo), t), t.perOctave);
}

// With it off: Fat 0…1 with the Matrix's sum, and the keyfollow, each held; its depths squared
void fatWords(const uint8_t* input, const Vc2OscConvertIn& in, const Vc2OscFatTerms& f, Vc2OscAnalogWords& w,
              const Vc2OscConvertTables& t) {
    w.subOn = 0.0f;
    const float x = add(div(flt(in.sums.osc[kOscFat]), t.sumScale), div(flt(s8(input, kFatAt)), t.beamScale));
    const float held = vc2OscHeld(x, 0.0f, 1.0f);
    const float follow = div(flt(s16(input, kFatKfAt) * f.k), t.perOctave);
    w.fat = vc2OscHeld(add(follow, held), 0.0f, 1.0f);
    w.fatEnv = mul(vc2OscDepthSquared(s8(input, kFatEnvAt), t), f.velocity);
    w.fatLfo = vc2OscDepthSquared(f.lfo, t);
}
} // namespace

void vc2OscAnalogConvert(const uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared,
                         Vc2OscAnalogWords& w) {
    const Vc2OscConvertTables& t = vc2OscConvertTables();
    const size_t osc = u8(input, kOscAt) == 0 ? 0 : 1;
    const int wave = u8(input, kWaveAt), k = (int16_t)(u8(input, kKeyAt) - kCentre);
    w.code = codeOf(wave);
    w.impact = div(mul(flt(s8(input, kImpactAt)), t.impactScale), t.impactPer);
    w.slope = aliasSlope(wave, in.system, t);
    w.width = widthOf(s8(input, kWidthAt), in.sums.osc[kOscWidth], s16(input, kWidthKfAt), k, in, t);
    w.widthEnvWord = kWidthEnvWords[osc];
    const float widthVelocity = vc2VelocityFactor(input, shared, kWidthSensAt, kWidthCurveAt, t.velocityScale);
    w.widthEnv = mul(vc2OscDepthSquared(s8(input, kWidthEnvAt), t), widthVelocity);
    w.wordD8 = 0;
    const int widthLfo = vc2OscHeld(in.sums.lfo[kLfoWidth] + s8(input, kWidthLfoAt), -kDepthMost, kDepthMost);
    w.widthLfo = vc2OscDepthSquared(widthLfo, t);
    w.fatEnvWord = kFatEnvWords[osc];
    w.wordC4 = 0;
    const Vc2OscFatTerms terms { k, vc2VelocityFactor(input, shared, kFatSensAt, kFatCurveAt, t.velocityScale),
                                 vc2OscHeld(in.sums.lfo[kLfoFat] + s8(input, kFatLfoAt), -kDepthMost, kDepthMost) };
    const auto words = u8(input, kSubOctaveAt) != 0 ? subWords : fatWords;
    words(input, in, terms, w, t);
    w.subLevel = div(flt(vc2OscLevelWord(t, u8(input, kSubLevelAt))), t.velocityScale);
    const bool square = wave == kLaSquare;
    w.tone = flt(square ? in.system.squareTone : in.system.sawTone);
    w.toneKf = flt(square ? in.system.squareToneKf : in.system.sawToneKf);
}
