// The OSC block's converter: see Vc2OscConvert.h (v251).  Each routine's arithmetic in its own order, in XtFloat; the
// integer sums that can pass 2^31 in uint32_t, as the SH-4's registers hold them.  Every address is rom.4's.
#include "Vc2OscConvert.h"
#include "Vc2InputReads.h"
#include "Vc2OscConvertTables.h"
#include "XtFloat.h"
#include <cstring>

namespace {
using vc2in::s16;
using vc2in::s32;
using vc2in::s8;
using vc2in::u8;
using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;

// [FW] the input's words, by its offset: the header's OSC and key; the record's Wave Gain, Pitch and its KF, the pitch's
// LFO and envelope depths with the envelope's velocity curve and sens, Level and its KF, the level's LFO depth and
// velocity curve and sens; the note's coarse, fine and second key; where rom.4 writes the two pitch10s
constexpr int kOscAt = 2, kKeyAt = 3, kWaveGainAt = 0x0a, kPitchAt = 0x0c, kPitchKfAt = 0x0e, kPitchLfoAt = 0x10;
constexpr int kPitchEnvAt = 0x11, kPitchCurveAt = 0x12, kPitchSensAt = 0x14, kLevelAt = 0x1c, kLevelKfAt = 0x1e;
constexpr int kLevelLfoAt = 0x20, kLevelCurveAt = 0x21, kLevelSensAt = 0x22, kWaveAt = 0x28, kCoarseAt = 0x88;
constexpr int kFineAt = 0x90, kPitch10At = 0x94, kKeyPitch10At = 0x98, kSecondKeyAt = 0x9c;
constexpr int kCentre = 60, kPerKey = 1000;      // [FW] the key follow's centre key; pitch10 a key
constexpr uint32_t kA4 = 69000;                  // [FW] 0x8939d080 (−69000 added): pitch10 at A4
constexpr int kLevelTop = 127, kDepthMost = 63;  // [FW] immediates
constexpr int kKfDivisor = 6000;                 // [FW] 0x8939c0e0's magic division (0x057619f1 >> 39), C's
constexpr int32_t kPitchEnvWords[2] = { 1, 5 }, kLevelEnvWords[2] = { 4, 8 };   // [FW] by OSC
constexpr int kPitch = 0, kLevel = 3, kLfoPitch = 1, kLfoLevel = 4;   // the groups' sums the part reads

// The key follow's sum, tenths of a cent before the fine: rom.2's 0x894e1250's law.  rom.4 (0x8939c8a0, 0x8939c910 and
// the +0x98 inline) forms KF 0 and 1000 (100%) without the product, which gives the same integer at both
int keySum(int key, int coarse, int kf) { return (kCentre + coarse) * kPerKey + (key - kCentre) * kf; }

// 0x8939c910: the key Pitch KF gives, the sum rounded up a key (the magic division by 1000 is C's), within 0…127; at
// KF 0 and 1000 rom.4 takes the key without the rounding, the same key once held to 0…127
uint8_t followedKey(int key, int coarse, int kf) {
    return (uint8_t)vc2OscHeld((keySum(key, coarse, kf) + kPerKey - 1) / kPerKey, 0, kLevelTop);
}

// The inline cents (and 0x8939c150's): pitch10 against A4, ÷ 10, held to ±6000, the upper bound first
float centsFromA4(uint32_t p10, const Vc2OscConvertTables& t) {
    return vc2OscHeld(div(flt((int32_t)(p10 - kA4)), t.perTenth), t.centsLeast, t.centsMost);
}

// 0x8939c1a0: a depth −63…+63 in cents, its table to 12, then its lines, the sign put back
float centsCurve(float x, const Vc2OscConvertTables& t) {
    int sign = 1;
    if (0.0f > x) {
        sign = -1;
        x = -x;
    }
    if (!(x > t.tableTop)) return mul(flt(t.cents[(size_t)xtf::ftrc(x)]), flt(sign));
    return mul(vc2OscOnLines(x, t.lines), flt(sign));
}

// 0x8939c0e0: the level with its keyfollow, level + trunc(127·kf·k/6000) as a halfword, within 0…127
int levelIndex(int level, int kf, int k) {
    const int follow = kf * k * kLevelTop / kKfDivisor;   // |kf·k·127| < 2^30: s16 × (−60…195) × 127
    return vc2OscHeld((int16_t)(follow + level), 0, kLevelTop);
}

// 0x8939c9a0: the calibration's gain at pitch10, the level curve at the row's Level × max(0, 1 + d·KF/6e6), d the
// pitch10 from the bias point, the upper KF above it and the lower (on −d) below
float calibrationGain(int row, uint32_t p10, const Vc2OscConvertIn& in, const Vc2OscConvertTables& t) {
    const Vc2OscCalibration& c = in.calibration[(size_t)(row > kVc2OscCalibrationRows - 1 ? 0 : row)];
    const uint32_t d = p10 - (uint32_t)(c.bias * kPerKey);
    const uint32_t step = (int32_t)d >= 0 ? d * (uint32_t)c.upper : (0u - d) * (uint32_t)c.lower;
    float g = add(div(flt((int32_t)step), t.kfScale), 1.0f);
    if (0.0f > g) g = 0.0f;
    return mul(div(flt(vc2OscLevelWord(t, c.level)), t.levelScale), g);
}

// The pitch's words: the two pitch10s into the input, the key, the cents, the ratio
void pitchWords(uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared, Vc2OscBlockHead& b,
                const Vc2OscConvertTables& t) {
    const int key = u8(input, kKeyAt), coarse = s16(input, kCoarseAt), kf = s16(input, kPitchKfAt);
    const uint32_t fine = (uint32_t)s32(input, kFineAt), p10 = (uint32_t)keySum(key, coarse, kf) + fine;
    const uint32_t keyP10 = (uint32_t)keySum(u8(input, kSecondKeyAt), coarse, kf) + fine;
    std::memcpy(input + kPitch10At, &p10, sizeof p10);
    std::memcpy(input + kKeyPitch10At, &keyP10, sizeof keyP10);
    b.key = followedKey(key, coarse, kf);
    b.cents = centsFromA4(p10, t);
    b.keyCents = centsFromA4(keyP10, t);
    const float x = add(div(mul(flt(in.sums.osc[kPitch]), t.depthMost), t.sumScale), flt(s8(input, kPitchAt)));
    const float octaves = div(centsCurve(vc2OscHeld(x, t.depthLeast, t.depthMost), t), t.perOctave);
    const int beam = 2 * in.beam[1] - in.beam[0];
    b.ratio = mul(shared.pow(2.0f, add(div(flt(beam), t.beamScale), octaves)), in.pitchSource);
}

// The level's words: its curve at Level with the Matrix's sum and its KF, × the velocity factor squared; its LFO depth
void levelWords(const uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared, Vc2OscBlockHead& b,
                const Vc2OscConvertTables& t) {
    float velocity = vc2VelocityFactor(input, shared, kLevelSensAt, kLevelCurveAt, t.velocityScale);
    velocity = mul(velocity, velocity);
    const int level = vc2OscHeld(in.sums.osc[kLevel] + u8(input, kLevelAt), 0, kLevelTop);
    const int index = levelIndex(level, s16(input, kLevelKfAt), (int16_t)(u8(input, kKeyAt) - kCentre));
    b.level = mul(div(flt(vc2OscLevelWord(t, index)), t.velocityScale), velocity);
    b.word58 = 0;
    const int d = vc2OscHeld(in.sums.lfo[kLfoLevel] + s8(input, kLevelLfoAt), -kDepthMost, kDepthMost);
    b.levelLfoDepth = vc2OscDepthSquared(d, t);
}
} // namespace

int vc2OscHeld(int v, int least, int most) {
    if (v > most) return most;
    return v >= least ? v : least;
}

float vc2OscHeld(float v, float least, float most) {
    if (v > most) return most;
    return least > v ? least : v;
}

float vc2OscOnLines(float x, std::span<const Vc2OscCentsLine> lines) {
    size_t line = 0;
    while (line + 1 < lines.size() && x > lines[line].top) ++line;
    return add(mul(x, lines[line].slope), lines[line].offset);
}

float vc2OscDepthSquared(int depth, const Vc2OscConvertTables& t) {
    const float q = div(flt(depth), t.depthMost);
    return mul(depth >= 0 ? q : -q, q);
}

int vc2OscAnalogRow(const uint8_t* input) { return u8(input, kWaveAt); }

void vc2OscBlockConvert(uint8_t* input, int row, const Vc2OscConvertIn& in, const Vc2ToneShared& shared,
                        Vc2OscBlockHead& b) {
    const Vc2OscConvertTables& t = vc2OscConvertTables();
    const size_t osc = u8(input, kOscAt) == 0 ? 0 : 1;
    pitchWords(input, in, shared, b, t);
    b.pitchEnvWord = kPitchEnvWords[osc];
    const float velocity = vc2VelocityFactor(input, shared, kPitchSensAt, kPitchCurveAt, t.velocityScale);
    b.pitchEnvDepth = mul(centsCurve(flt(s8(input, kPitchEnvAt)), t), velocity);
    b.word40 = 0;
    const int d = vc2OscHeld(in.sums.lfo[kLfoPitch] + s8(input, kPitchLfoAt), -kDepthMost, kDepthMost);
    b.pitchLfoDepth = centsCurve(flt(d), t);
    const float gain = shared.pow(2.0f, div(flt(s8(input, kWaveGainAt)), t.gainScale));
    b.gain = mul(calibrationGain(row, (uint32_t)s32(input, kPitch10At), in, t), gain);
    b.levelEnvWord = kLevelEnvWords[osc];
    levelWords(input, in, shared, b, t);
}
