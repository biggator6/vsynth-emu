// SOLO's frame on the OSC voice (v334): rom.4's 0x893c97b0…, rom.2's 0x8950d0f0 on rom.4's object (Vc2OscSolo.h):
// the two grains' windows and reads (0x893c9a40, held to the last sample by 0x893c9ae0), mono or stereo by byte 0,
// a second tap a period on when its weights sum above 0, the windows' mix, the Energy filter, the gain.
// HARDWARE_FIRMWARE.md, "The VC-2's SOLO".  Every address is rom.4's, every offset the object's.  Audio thread.
#include "Vc2OscSolo.h"
#include "Vc2OscTables.h"
#include "XtFloat.h"

namespace vc2solo {
namespace {
using xtf::add;
using xtf::flt;
using xtf::mul;
using xtf::sub;
constexpr uint8_t kStereo = 0x40;   // [FW] byte 0's
constexpr uint32_t kSixteenths = 4, kLowBits = 15, kFrameBytes = 4, kHalfBytes = 2;

// [FW] a grain's window: its place held to 1, its gain the place (below ½) or 1 less it, × 2; the place a step on
float window(Vc2WalkWords& w, uint32_t at, float step) {
    const Vc2OscSoloPool& k = vc2OscTables().solo;
    float place = w.real(at);
    if (place > 1.0f) place = 1.0f;
    const float gain = k.windowHalf > place ? place : sub(1.0f, place);
    w.setReal(at, add(place, step));
    return mul(gain, k.windowScale);
}
// [FW] a grain's read: its age a sample on, the age × its weight in samples plus its start's sixteenths, split into a
// sample (the start's own on it) and a fraction
void readOf(Vc2WalkWords& w, const Vc2OscSoloGrain& g, int32_t& at, float& frac) {
    const Vc2OscSoloPool& k = vc2OscTables().solo;
    const int32_t age = w.word(g.age);
    w.setWord(g.age, vc2WalkPlus(age, kAgeStep));
    const uint32_t start = (uint32_t)w.word(g.start);
    float x = mul(mul(flt(age), w.real(g.weight)), k.ageUnit);
    x = add(mul(k.sixteenth, flt((int)(start & kLowBits))), x);
    const int32_t whole = xtf::ftrc(x);
    frac = sub(x, flt(whole));
    at = vc2WalkPlus(whole, (int32_t)(start >> kSixteenths));
}

// [FW] between a sample and the next by the fraction (FLOAT, FSUB, FMAC)
float between(int a, int b, float f) {
    const float s = flt(a);
    return add(mul(sub(flt(b), s), f), s);
}
// One channel of the two grains' reads, its samples `bytes` apart, from the right half word on a stereo frame's
struct Pair {
    float a, b;
};
Pair readPair(const Vc2WalkWords& w, const Vc2OscSamples& wave, const Vc2OscSoloTaps& t, uint32_t frame,
              uint32_t side) {
    const uint32_t from = (uint32_t)w.word(kSamples) + side;
    const uint32_t at = from + (uint32_t)t.a * frame, bt = from + (uint32_t)t.b * frame;
    return { between(wave.at(at), wave.at(at + frame), t.fa), between(wave.at(bt), wave.at(bt + frame), t.fb) };
}

// [FW] the second tap (0x893c97fe…, 0x893c9930…): each grain's fraction a period on (×1/16), split again, held to the
// last sample
Vc2OscSoloTaps secondTaps(const Vc2WalkWords& w, Vc2OscSoloTaps t) {
    const float sixteenth = vc2OscTables().solo.sixteenth;
    const auto on = [&](int32_t& at, float& frac, uint32_t period) {
        frac = add(mul(sixteenth, flt(w.word(period))), frac);
        const int32_t whole = xtf::ftrc(frac);
        at = vc2WalkPlus(at, whole);
        frac = sub(frac, flt(whole));
    };
    on(t.a, t.fa, kGrainA.period);
    on(t.b, t.fb, kGrainB.period);
    clampTaps(w, t);
    return t;
}

// [FW] the Energy filter on one channel's sample, its two states (0x893c9868…, 0x893c99e6…)
float filtered(Vc2WalkWords& w, float x, uint32_t band, uint32_t low) {
    const float c = w.real(kCoef), q = w.real(kQ), s1 = w.real(band);
    const float s2 = add(mul(c, s1), w.real(low));
    w.setReal(low, s2);
    const float y = sub(sub(x, mul(q, s1)), s2);
    w.setReal(band, add(mul(c, y), s1));
    return y;
}
// [FW] the gain a glide toward its target by its rate (FMAC), × 65536
float glided(Vc2WalkWords& w) {
    const float gain = w.real(kGain);
    const float g = add(mul(w.real(kGainRate), sub(w.real(kTarget), gain)), gain);
    w.setReal(kGain, g);
    return mul(g, vc2OscTables().solo.sampleScale);
}
} // namespace

// [FW] 0x893c9ae0: a read at or past the last sample held there, its fraction 0
void clampTaps(const Vc2WalkWords& w, Vc2OscSoloTaps& t) {
    const int32_t last = w.word(kLastSample);
    for (auto [at, frac] : { std::pair { &t.a, &t.fa }, std::pair { &t.b, &t.fb } }) {
        if (last > *at) continue;
        *at = last;
        *frac = 0.0f;
    }
}

// [FW] 0x893c9a40: the windows (A's, then B's, by the windows' step), then the reads
Vc2OscSoloTaps grains(Vc2WalkWords& w) {
    Vc2OscSoloTaps t;
    const float step = w.real(kWindowStep);
    t.wa = window(w, kGrainA.window, step);
    t.wb = window(w, kGrainB.window, step);
    readOf(w, kGrainA, t.a, t.fa);
    readOf(w, kGrainB, t.b, t.fb);
    clampTaps(w, t);
    return t;
}

// [FW] 0x893c97b0…: the grains; each channel's two reads (a stereo frame's left half word, then its right); with the
// two taps' weights summing above 0, each read moved toward its second tap's by its weight (B's weight the sum less
// A's, as FADD then FSUB leave it); the windows' mix, the filter, the gain; mono both sides the one channel
void frame(Vc2WalkWords& w, const Vc2OscSamples& wave, float* lr) {
    const Vc2OscSoloTaps t = grains(w);
    const bool stereo = (w.byte(vc2walk::kFlags) & kStereo) != 0;
    const uint32_t bytes = stereo ? kFrameBytes : kHalfBytes;
    const float tapA = w.real(kGrainA.tap), sum = add(w.real(kGrainB.tap), tapA);
    const float tapB = sub(sum, tapA);
    const Vc2OscSoloTaps second = sum > 0.0f ? secondTaps(w, t) : t;
    for (uint32_t side = 0; side < (stereo ? 2u : 1u); ++side) {
        Pair p = readPair(w, wave, t, bytes, side * kHalfBytes);
        if (sum > 0.0f) {
            const Pair q = readPair(w, wave, second, bytes, side * kHalfBytes);
            p = { add(mul(tapA, sub(q.a, p.a)), p.a), add(mul(tapB, sub(q.b, p.b)), p.b) };
        }
        const float mixed = add(mul(t.wb, p.b), mul(p.a, t.wa));
        lr[side] = filtered(w, mixed, side == 0 ? kBandLeft : kBandRight, side == 0 ? kLowLeft : kLowRight);
    }
    const float g = glided(w);
    lr[0] = mul(lr[0], g);
    lr[1] = stereo ? mul(lr[1], g) : lr[0];
}
} // namespace vc2solo
