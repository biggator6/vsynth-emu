// SOLO's render on the OSC voice (v334): rom.4's 0x893c9450, rom.2's 0x8950cd90 on rom.4's object (Vc2OscSolo.h):
// the beat's block, the block's rates, the next crossing, then 32 frames, each its step (the position, the walk's
// crossings, the segments, the anchor and its records, the grain trigger) and its reads (Vc2OscSoloFrame.cpp).
// HARDWARE_FIRMWARE.md, "The VC-2's SOLO".  Every address is rom.4's, every offset the object's.  Audio thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscSolo.h"
#include "Vc2OscTables.h"
#include "XtFloat.h"

namespace vc2solo {
namespace {
using namespace vc2walk;
using xtf::add;
using xtf::mul;
using xtf::sub;
constexpr int kFrames = 32, kFracBits = 20;   // [FW] a block; the position's fraction's bits
constexpr uint32_t kFracMask = 0xfffff;
constexpr uint8_t kWraps = 0x08;   // [FW] byte 0's: the walk wraps at an end (with the range by events)

int32_t minus(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

// [FW] 0x893c94a0: the rate (the pitch, × the rate factor but under Robot Voice); the filter's Q and its cutoff by the
// Energy (0: 1.2 and the cutoff × ⅛; else 1.2 less the Energy, lifted between the rates ½ and 2), its coefficient
// a cubic of the cutoff; both 0.125 of the way there, the coefficient held under the most less the Q
void blockRates(Vc2WalkWords& w) {
    const Vc2OscSoloPool& k = vc2OscTables().solo;
    float rate = w.real(kPitch);
    if ((w.byte(kFlags) & kRobot) == 0) rate = mul(rate, w.real(kRate));
    w.setReal(kRateNow, rate);
    const float energy = w.real(kEnergy);
    float cutoff = w.real(kCutoff);
    float q = k.offQ;
    if (energy == 0.0f) {
        cutoff = mul(cutoff, k.offScale);
    } else {
        float lift = 0.0f;
        if (!(k.liftLow > rate) && 1.0f > rate) lift = mul(sub(mul(k.liftTop, rate), 1.0f), energy);
        if (!(k.liftLow > rate) && !(1.0f > rate) && k.liftTop > rate) lift = mul(sub(k.liftTop, rate), energy);
        q = add(sub(k.offQ, energy), lift);
    }
    const float cubic = mul(mul(k.cubic, cutoff), cutoff);
    const float c = mul(mul(cutoff, sub(k.cubicBase, cubic)), k.coefScale);
    const float coefWas = w.real(kCoef), qWas = w.real(kQ);
    float coef = add(mul(k.glide, sub(c, coefWas)), coefWas);
    const float qNow = add(mul(k.glide, sub(q, qWas)), qWas);
    const float most = sub(k.sumMost, qNow);
    w.setReal(kQ, qNow);
    if (coef > most) coef = most;
    w.setReal(kCoef, coef);
}

// [FW] 0x893c95ec… and 0x893c96bc…: whether the budget allows the record again: above 0, and all its bits or the
// repeat's cost taken from it
bool repeats(Vc2WalkWords& w) {
    const int32_t budget = w.half(kBudget);
    if (!(budget > 0)) return false;
    if (budget == kUnlimited) return true;
    const int32_t cost = repeatCost(w);
    if (!(budget >= cost)) return false;
    w.setHalf(kBudget, budget - cost);
    return true;
}

// [FW] 0x893c960a: the anchor past the record's end wrapped back into it (more than once when the step is longer)
void wrapForward(Vc2WalkWords& w, int32_t anchor, int32_t end) {
    int32_t at = minus(anchor, end), by = w.word(kRecStart);
    bool again = true;
    while (again) {
        at = vc2WalkPlus(at, by);
        again = at >= end;
        by = minus(by, end);
    }
    w.setWord(kAnchor, at);
}
// [FW] 0x893c96da: below the record's start, up into it
void wrapBackward(Vc2WalkWords& w, int32_t anchor, int32_t start) {
    int32_t at = minus(anchor, start), by = w.word(kRecEnd);
    bool again = true;
    while (again) {
        at = vc2WalkPlus(at, by);
        again = start >= at;
        by = minus(by, start);
    }
    w.setWord(kAnchor, at);
}

// [FW] 0x893c95d2: the anchor a step on; past the record's end, the record again if the position hasn't passed it and
// the budget allows, or it reaches the range's end; else the next records while the position is past them
void anchorForward(Vc2WalkWords& w) {
    const int32_t anchor = vc2WalkPlus(w.word(kAnchor), w.word(kAnchorStep));
    w.setWord(kAnchor, anchor);
    const int32_t end = w.word(kRecEnd);
    if (!(anchor >= end)) return;
    if ((end > w.word(kPos) && repeats(w)) || end >= w.word(kRangeEnd)) {
        wrapForward(w, anchor, end);
        return;
    }
    while (loadNext(w) > 0 && w.word(kPos) >= w.word(kRecEnd)) {
        w.setWord(kAnchor, minus(vc2WalkPlus(w.word(kAnchor), w.word(kRecEnd)), w.word(kRecStart)));
    }
}
// [FW] 0x893c96ae: a step back; below the record's start, the record again if the position is in it and the budget
// allows, or it starts at the range's start (0 once the backward loop turned the walk); else the records before
void anchorBackward(Vc2WalkWords& w) {
    const int32_t anchor = minus(w.word(kAnchor), w.word(kAnchorStep));
    w.setWord(kAnchor, anchor);
    const int32_t start = w.word(kRecStart);
    if (!(start > anchor)) return;
    const int32_t floor = (w.byte(kWalkBits) & kFromZero) != 0 ? 0 : w.word(kRangeStart);
    if ((w.word(kPos) >= start && repeats(w)) || floor >= start) {
        wrapBackward(w, anchor, start);
        return;
    }
    while (loadPrev(w) > 0 && w.word(kRecStart) > w.word(kPos)) {
        w.setWord(kAnchor, minus(vc2WalkPlus(w.word(kAnchor), w.word(kRecStart)), w.word(kRecEnd)));
    }
}

// [FW] 0x893c9568… and 0x893c9668…: at the range's end with the wrap on and the range by events, to its far event
void wrapRange(Vc2WalkWords& w, uint32_t to) {
    w.setHalf(kIndex, w.half(to));
    kVc2SoloWalk.jump(w.v, w.p);
    vc2WalkCrossing(w.v, w.p);
}
bool wrapsAt(const Vc2WalkWords& w) { return (w.byte(kFlags) & (kWraps | kInSamples)) == kWraps; }

// [FW] 0x893c955c…: forward to the position; at the range's end ended there (or wrapped); past the next crossing, the
// walk's; past the next segment's start, into it with its first record, unless that reaches the range's end; the anchor
void forward(Vc2WalkWords& w, int32_t pos, int32_t frac) {
    const int32_t end = w.word(kRangeEnd);
    if (pos >= end && wrapsAt(w)) return wrapRange(w, kRangeFirst);
    if (pos >= end) {
        w.setByte(kEnded, 1);
        pos = end;
        frac = 0;
    }
    w.setWord(kPos, pos);
    w.setWord(kFrac, frac);
    if (pos >= w.word(kCross)) {
        vc2WalkForward(w.v, w.p, kVc2SoloWalk);
        vc2WalkCrossing(w.v, w.p);
        pos = w.word(kPos);
    }
    const int32_t next = w.word(kNextSegment);
    const uint32_t first = (uint32_t)w.word(kNextFirst);
    if (pos >= next && w.word(kRangeEnd) > vc2WalkPlus(next, w.memoryHalf(first))) {
        w.setWord(kSegment, next);
        w.setWord(kRecEnd, next);
        w.setWord(kNextRecord, (int32_t)first);
        w.setWord(kAnchor, next);
        segmentEntry(w, (uint32_t)w.word(kEntryAfter));
        loadNext(w);
    }
    anchorForward(w);
}
// [FW] 0x893c9654…: back; at the range's start (0 once the backward loop turned the walk) ended there (or wrapped);
// at or before the next crossing, the walk's, then compared with the crossing (rom.4's r1 after 0x893c9380); below the
// segment's start, the segment before it; the anchor
void backward(Vc2WalkWords& w, int32_t pos, int32_t frac) {
    const int32_t start = (w.byte(kWalkBits) & kFromZero) != 0 ? 0 : w.word(kRangeStart);
    if (start >= pos && wrapsAt(w)) return wrapRange(w, kRangeLast);
    if (start >= pos) {
        w.setByte(kEnded, kBefore);
        pos = start;
        frac = 0;
    }
    w.setWord(kPos, pos);
    w.setWord(kFrac, frac);
    if (w.word(kCross) >= pos) {
        vc2WalkBackward(w.v, w.p, kVc2SoloWalk);
        vc2WalkCrossing(w.v, w.p);
        pos = w.word(kCross);
    }
    constexpr uint32_t kEntryBytes = 12, kBack = 3 * kEntryBytes;   // [FW] a segment entry; from the entry after next
    if (w.word(kSegment) > pos) {
        const uint32_t from = (uint32_t)w.word(kEntryAfter) - kBack;
        w.setWord(kSegment, segmentEntry(w, from));
        segmentEntry(w, from + kEntryBytes);
    }
    anchorBackward(w);
}

// [FW] 0x893c9740: the trigger's timer a sample on; past its period, the period off it and, before the end, the next
// grain from the parameters: by the toggle A or B, its age the timer, its window's place the age × the windows'
// step, its start the record's and the offset, its weights the parameters', its period the record's
void trigger(Vc2WalkWords& w) {
    const int32_t timer = vc2WalkPlus(w.word(kTimer), kAgeStep);
    const int32_t period = w.word(kTrigger);
    w.setWord(kTimer, timer);
    if (!(timer >= period)) return;
    w.setWord(kTimer, minus(timer, period));
    if (w.byte(kEnded) != 0) return;
    grainParams(w);
    const float place = mul(mul(xtf::flt(w.word(kTimer)), vc2OscTables().solo.ageUnit), w.real(kWindowStep));
    const uint8_t toggle = (uint8_t)(w.byte(kToggle) ^ 1);
    w.setByte(kToggle, toggle);
    const Vc2OscSoloGrain& g = (toggle & 1) != 0 ? kGrainB : kGrainA;
    w.setWord(g.age, w.word(kTimer));
    w.setReal(g.window, place);
    w.setWord(g.start, vc2WalkPlus(w.word(kRecStart), w.word(kOffset)));
    w.setWord(g.weight, w.word(kWeight));
    w.setWord(g.tap, w.word(kTap));
    w.setWord(g.period, w.word(kRecLength));
}

// [FW] 0x893c9540: half the step onto the position's fraction; then forward, back, or (still) the anchor by the side
// the position is on; then the trigger
void step(Vc2WalkWords& w) {
    const int32_t half = w.word(kStep) >> 1;
    const int32_t frac = vc2WalkPlus(w.word(kFrac), half);
    const int32_t pos = vc2WalkPlus(w.word(kPos), frac >> kFracBits);
    const int32_t kept = (int32_t)((uint32_t)frac & kFracMask);
    if (half != 0) (half > 0 ? forward : backward)(w, pos, kept);
    else (w.word(kPos) >= w.word(kAnchor) ? anchorForward : anchorBackward)(w);
    trigger(w);
}
} // namespace
} // namespace vc2solo

// [FW] 0x893c9450: the beat's block (0x893c89a0, Vc2OscVoice::beatBlock) gives the step's word; the block's rates; the
// next crossing (0x893c9380); then 32 frames, each stepped, then read
void vc2OscSolo(Vc2OscVoice& v, float* out, Vc2OscParts& p) {
    Vc2WalkWords w { v, p };
    v.beatBlock(p);
    vc2solo::blockRates(w);
    vc2WalkCrossing(v, p);
    const Vc2OscSamples wave = p.waveSamples();
    for (int k = 0; k < vc2solo::kFrames; ++k) {
        vc2solo::step(w);
        vc2solo::frame(w, wave, out + 2 * k);
    }
}
