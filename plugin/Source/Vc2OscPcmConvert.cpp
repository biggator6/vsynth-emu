// PCM's OSC converter's own part: see Vc2OscPcmConvert.h (v394).  Each routine's arithmetic in its own order, in
// XtFloat; FMAC as its product then its sum.  Every address is rom.4's.
#include "Vc2OscPcmConvert.h"
#include "Vc2InputReads.h"
#include "Vc2OscConvertTables.h"
#include "Vc2OscExtinConvert.h"
#include "XtFloat.h"
#include <bit>
#include <cstring>
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

constexpr size_t kInputBytes = 0x108;   // [FW] an OSC converter's input
// [FW] the input's words: the OSC and key; Wave Gain; the Time's depth, keyfollow, LFO and envelope depths, velocity
// curve and sens; the Formant's the same; the Time's mode; the wave's two level bytes
constexpr int kOscAt = 2, kKeyAt = 3, kGainAt = 0x0a;
constexpr int kTimeAt = 0x4c, kTimeKfAt = 0x4e, kTimeLfoAt = 0x50, kTimeEnvAt = 0x51, kTimeCurveAt = 0x52;
constexpr int kTimeSensAt = 0x54, kFormantAt = 0x56, kFormantKfAt = 0x58, kFormantLfoAt = 0x5a, kFormantEnvAt = 0x5b;
constexpr int kFormantCurveAt = 0x5c, kFormantSensAt = 0x5e, kTimeModeAt = 0x65, kWaveLevelAt = 0xa4;
constexpr int kWaveLevelTwoAt = 0xa6;
constexpr int kCentre = 60, kDepthMost = 63;                 // [FW] the key's centre; a depth's hold
constexpr int kOscTime = 1, kOscFormant = 2, kLfoTime = 2, kLfoFormant = 3;   // the groups' sums
constexpr float kTwo = 2.0f;                                 // fldi1, added to itself
constexpr float kSigns[3] = { -1.0f, 0.0f, 1.0f };           // [FW] the Time's mode 0…2 (fldi1 fneg, fldi0, fldi1)
constexpr float kSignPast = 1.0f;                            // [FW] past 2, fldi1
constexpr int32_t kTimeEnvWords[2] = { 2, 6 }, kFormantEnvWords[2] = { 3, 7 };   // [FW] +0x28 and +0x30, by OSC
// [FW] the block's words by their index
constexpr size_t kTimeWord = 0x10 / 4, kFormantWord = 0x14 / 4, kTimeEnvDepth = 0x24 / 4, kTimeEnvWord = 0x28 / 4;
constexpr size_t kFormantEnvDepth = 0x2c / 4, kFormantEnvWord = 0x30 / 4, kTimeLfoDepth = 0x44 / 4;
constexpr size_t kTimeLfoZero = 0x48 / 4, kFormantLfoDepth = 0x4c / 4, kFormantLfoZero = 0x50 / 4, kGainWord = 0x5c / 4;

const Vc2OscConvertTables& pcmTables() { return vc2OscConvertTables(); }

uint32_t pcmWord(float f) { return std::bit_cast<uint32_t>(f); }

int16_t fromCentre(const uint8_t* input) { return static_cast<int16_t>(u8(input, kKeyAt) - kCentre); }

// A Time depth on its curve: past +60 the line x × 2 − 120, ÷ 3, + 3; past −60 x × −2 − 120, ÷ −3, − 3; else x ÷ 20
float timeDepthCurve(float x) {
    const Vc2OscConvertTables& t = pcmTables();
    if (x > t.depthKnee) return add(div(add(mul(x, kTwo), t.depthOffset), t.depthPer), t.depthPer);
    if (t.depthKneeLow > x) return add(div(add(mul(x, t.depthSlopeLow), t.depthOffset), t.depthPerLow), t.depthPerLow);
    return add(div(x, t.depthScale), 0.0f);
}

// The Time law's lines past ±60: from the sign ± 3, to ±5 over the 3 past the knee
float timeUpper(float x, float sign) {
    const Vc2OscConvertTables& t = pcmTables();
    const float from = add(sign, t.timeFrom);
    return add(div(mul(sub(t.timeMostOut, from), add(x, t.timeKneeLow)), t.timeFrom), from);
}

float timeLower(float x, float sign) {
    const Vc2OscConvertTables& t = pcmTables();
    const float from = add(sign, t.timeFromLow);
    return add(from, div(mul(sub(t.timeLeastOut, from), add(x, t.timeKnee)), t.timeFromLow));
}

// The Time law's curve: held to ±63, a line past ±60, else x ÷ 20 + the sign
float timeCurve(float x, float sign) {
    const Vc2OscConvertTables& t = pcmTables();
    if (x > t.formantDepth) return timeUpper(t.formantDepth, sign);
    if (t.timeLeast > x) return timeLower(t.timeLeast, sign);
    if (x > t.timeKnee) return timeUpper(x, sign);
    if (t.timeKneeLow > x) return timeLower(x, sign);
    return add(div(x, t.timeScale), sign);
}

// A depth with a Matrix sum, held to ±63
int heldDepth(int depth, int sum) { return vc2OscHeld(depth + sum, -kDepthMost, kDepthMost); }

// The words of v251's head that PCM's converter writes alike, from the shared part run on a copy of the input
void headWords(const uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared, uint32_t* block) {
    std::array<uint8_t, kInputBytes> copy;
    std::memcpy(copy.data(), input, copy.size());
    Vc2OscBlockHead made {};
    vc2OscBlockConvert(copy.data(), kVc2OscVocoderRow, in, shared, made);
    Vc2OscBlockHead head;
    std::memcpy(&head, block, sizeof head);
    head.ratio = made.ratio;
    head.pitchEnvDepth = made.pitchEnvDepth;
    head.pitchEnvWord = made.pitchEnvWord;
    head.level = made.level;
    head.levelEnvWord = made.levelEnvWord;
    head.pitchLfoDepth = made.pitchLfoDepth;
    head.word40 = made.word40;
    head.levelLfoDepth = made.levelLfoDepth;
    head.word58 = made.word58;
    std::memcpy(block, &head, sizeof head);
}

Vc2OscPcmTimeIn timeIn(const uint8_t* input, const Vc2OscConvertIn& in) {
    Vc2OscPcmTimeIn t;
    t.depth = static_cast<int16_t>(s8(input, kTimeAt));
    t.sum = in.sums.osc[kOscTime];
    t.follow = static_cast<int16_t>(s16(input, kTimeKfAt));
    t.key = fromCentre(input);
    t.beam = in.widthBeam;
    const uint8_t mode = u8(input, kTimeModeAt);
    t.sign = mode < std::size(kSigns) ? kSigns[mode] : kSignPast;
    return t;
}
} // namespace

float vc2OscPcmTime(const Vc2OscPcmTimeIn& in, const Vc2ToneShared& shared) {
    const Vc2OscConvertTables& t = pcmTables();
    const auto keyed = static_cast<int32_t>(static_cast<uint32_t>(in.key) * static_cast<uint32_t>(in.follow));
    const float scale = shared.pow(kTwo, div(flt(keyed), t.formantCents));
    const float x = add(div(mul(flt(in.sum), t.formantDepth), t.formantMost), flt(in.depth));
    const float y = timeCurve(x, in.sign);
    const int beamed = in.beam[1] - 3 * in.beam[0];
    const float out = mul(add(div(flt(beamed), t.timeBeam), y), scale);
    return vc2OscHeld(out, t.timeLeastOut, t.timeMostOut);
}

float vc2OscPcmFormant(int16_t depth, int16_t sum, int16_t follow, int16_t key) {
    const Vc2OscConvertTables& t = pcmTables();
    const auto keyed = static_cast<int32_t>(static_cast<uint32_t>(follow) * static_cast<uint32_t>(key));
    return vc2OscFormantHeld(depth, sum, div(flt(keyed), t.formantCents), t.pcmFormantLeast);
}

void vc2OscPcmConvert(uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared, uint32_t* block) {
    const Vc2OscConvertTables& t = pcmTables();
    headWords(input, in, shared, block);
    const int osc = u8(input, kOscAt) == 0 ? 0 : 1;
    block[kTimeWord] = pcmWord(vc2OscPcmTime(timeIn(input, in), shared));
    block[kTimeEnvWord] = static_cast<uint32_t>(kTimeEnvWords[osc]);
    const float timeVelocity = vc2VelocityFactor(input, shared, kTimeSensAt, kTimeCurveAt, t.pcmVelocityScale);
    block[kTimeEnvDepth] = pcmWord(mul(timeDepthCurve(flt(s8(input, kTimeEnvAt))), timeVelocity));
    block[kTimeLfoZero] = 0;
    block[kTimeLfoDepth] = pcmWord(timeDepthCurve(flt(heldDepth(s8(input, kTimeLfoAt), in.sums.lfo[kLfoTime]))));
    const auto formantDepth = static_cast<int16_t>(s8(input, kFormantAt));
    const auto formantFollow = static_cast<int16_t>(s16(input, kFormantKfAt));
    const int16_t key = fromCentre(input);
    block[kFormantWord] = pcmWord(vc2OscPcmFormant(formantDepth, in.sums.osc[kOscFormant], formantFollow, key));
    block[kFormantEnvWord] = static_cast<uint32_t>(kFormantEnvWords[osc]);
    const float formantVelocity =
        vc2VelocityFactor(input, shared, kFormantSensAt, kFormantCurveAt, t.pcmVelocityScale);
    block[kFormantEnvDepth] = pcmWord(mul(div(flt(s8(input, kFormantEnvAt)), t.pcmDepthTop), formantVelocity));
    block[kFormantLfoZero] = 0;
    const int formantLfo = heldDepth(s8(input, kFormantLfoAt), in.sums.lfo[kLfoFormant]);
    block[kFormantLfoDepth] = pcmWord(div(flt(formantLfo), t.pcmDepthTop));
    const float gain = shared.pow(kTwo, div(flt(s8(input, kGainAt)), t.pcmGainScale));
    const float first = div(flt(vc2OscLevelWord(t, u8(input, kWaveLevelAt))), t.pcmVelocityScale);
    const float second = div(flt(vc2OscLevelWord(t, u8(input, kWaveLevelTwoAt))), t.pcmVelocityScale);
    block[kGainWord] = pcmWord(mul(mul(first, gain), second));
}
