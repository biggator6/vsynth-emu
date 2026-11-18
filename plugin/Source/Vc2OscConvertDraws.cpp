// xt_vc2carrier's third part's draws for the analog converter's second part (v256), from a generator of their own, so
// that the first part's draws stay as v251 drew them: the record's analog bytes and halfwords; the Matrix's Pulse Width
// and Fat sums, wide half the time, past what their holds take; the LFO's two; M's bytes +0x180 and +0x181; the analog
// oscillator's system words.  XT_VC2CARRIER_MAIN only.
#ifdef XT_VC2CARRIER_MAIN
#include "Vc2CarrierCheck.h"
#include "Vc2OscConvert.h"
#include <cstring>

namespace {
constexpr uint32_t kSystemAt = 0x89e371e0 + 0x10205, kWidthBeamAt = 0x180;   // [FW]
constexpr uint32_t kSawSlopeAt = 5, kSquareSlopeAt = 7;                      // [FW] from kSystemAt
constexpr int kWaveAt = 0x28, kSubOctaveAt = 0x3f, kLastSlopeWave = 1, kLaSquare = 10;   // [FW]
constexpr int kOscWidth = 1, kOscFat = 2, kSumReach = 13000;   // past ±12000, where the sums' holds act
constexpr int kWidthAt = 0x2a, kWidthKfAt = 0x2c, kWidthTop = 63;   // [FW] the Pulse Width's byte, KF and top

// [FW] the record's bytes by the input's offset, and their ranges: Impact, Pulse Width, its LFO and envelope depths and
// the envelope's velocity curve, Fat, its LFO and envelope depths and velocity curve, Detune, Sub Level
constexpr Vc2ByteDraw kBytes[] = {
    { 0x29, 0, 40 }, { 0x2a, -63, 63 }, { 0x2e, -63, 63 }, { 0x2f, -63, 63 }, { 0x30, 0, 7 }, { 0x34, 0, 127 },
    { 0x38, -63, 63 }, { 0x39, -63, 63 }, { 0x3a, 0, 7 }, { 0x3e, -63, 63 }, { 0x40, 0, 127 },
};
// [FW] the halfwords: the Pulse Width's KF and velocity sens, Fat's the same
constexpr Vc2ByteDraw kHalfwords[] = { { 0x2c, -200, 200 }, { 0x32, -200, 200 }, { 0x36, -200, 200 },
                                       { 0x3c, -200, 200 } };
constexpr Vc2LfoEdges kLfoEdges { { 0x2e, 0x38 }, { 2, 3 } };   // [FW] the LFO's Pulse Width and Fat depths and sums

// A Matrix sum: none half the time, else a little, or across the hold
int16_t drawHeldSum(std::mt19937& rng) {
    if (rng() % 2 == 0) return 0;
    const bool wide = rng() % 2 == 0;
    return (int16_t)(wide ? (int)(rng() % (2 * kSumReach + 1)) - kSumReach : vc2DrawHalf(rng, -200, 200));
}
} // namespace

void vc2DrawAnalogRecord(std::mt19937& rng, uint8_t* in, Vc2OscConvertIn& w, Vc2OscAnalogCounts& n) {
    for (const Vc2ByteDraw& b : kBytes) in[b.at] = (uint8_t)vc2DrawByte(rng, b.lo, b.hi);
    for (const Vc2ByteDraw& h : kHalfwords) vc2Put16(in + h.at, vc2DrawHalf(rng, h.lo, h.hi));
    in[kSubOctaveAt] = (uint8_t)(rng() % 2 == 0 ? 0 : vc2DrawByte(rng, 1, 3));
    w.sums.osc[kOscWidth] = drawHeldSum(rng);
    w.sums.osc[kOscFat] = drawHeldSum(rng);
    int16_t lfo[5];
    vc2DrawLfoSums(rng, in, lfo, 70, kLfoEdges);
    for (const int sum : kLfoEdges.sum) w.sums.lfo[sum] = lfo[sum];
    if (rng() % 2 == 0) {
        for (uint8_t& b : w.widthBeam) b = (uint8_t)vc2DrawByte(rng, 0, 127);
    }
    if (rng() % 8 == 0) {   // the wrap's edge: the knob at its top with nothing else, the integer part exactly 126
        in[kWidthAt] = kWidthTop;
        vc2Put16(in + kWidthKfAt, 0);
        w.sums.osc[kOscWidth] = 0;
        w.widthBeam = {};
    }
    Vc2OscAnalogSystem& s = w.system;
    for (uint8_t* b : { &s.sawTone, &s.squareTone, &s.sawToneKf, &s.squareToneKf }) *b = (uint8_t)rng();
    s.aliasCancel = (uint8_t)vc2DrawByte(rng, 0, 1);
    for (uint16_t* slope : { &s.sawSlope, &s.squareSlope }) *slope = rng() % 2 == 0 ? 100 : (uint16_t)rng();
    ++n.conversions;
    n.sub += in[kSubOctaveAt] != 0;
    n.slope += s.aliasCancel != 0 && in[kWaveAt] <= kLastSlopeWave;
    n.laSquare += in[kWaveAt] == kLaSquare;
}

void vc2WriteAnalogWorld(Vc2Fw& fw, const Vc2OscConvertIn& w, uint32_t matrixAt) {
    const Vc2OscAnalogSystem& s = w.system;
    const uint8_t bytes[] = { s.sawTone, s.squareTone, s.sawToneKf, s.squareToneKf, s.aliasCancel };
    fw.cpu.write(matrixAt + kWidthBeamAt, w.widthBeam.data(), w.widthBeam.size());
    fw.cpu.write(kSystemAt, bytes, sizeof bytes);
    fw.cpu.write(kSystemAt + kSawSlopeAt, &s.sawSlope, sizeof s.sawSlope);
    fw.cpu.write(kSystemAt + kSquareSlopeAt, &s.squareSlope, sizeof s.squareSlope);
}
#endif
