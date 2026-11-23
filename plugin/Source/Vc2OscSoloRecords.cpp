// SOLO's records and grains' parameters on the OSC voice (v334): rom.4's 0x893c9b80 and 0x893c9bf0 (rom.2's 0x8950d4c0,
// 0x8950d530), 0x893c9c60 (0x8950d5a0), 0x893c9dd0 (0x8950d6f0) and the jumps 0x893c9e90 and 0x893c9eb0 (0x8950d790,
// 0x8950d7d0), SOLO's row of the walk (Vc2OscSolo.h); from v335 the records' walks, segment entries and jumps' records
// the record players' (Vc2OscRecordBlock.cpp) on SOLO's layout.  HARDWARE_FIRMWARE.md, "The VC-2's SOLO".  Every address
// is rom.4's, every offset the object's.  Audio thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscSolo.h"
#include "Vc2OscTables.h"
#include "XtFloat.h"

namespace vc2solo {
namespace {
using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;
using xtf::sub;
constexpr uint8_t kForward = 0x80;   // [FW] byte 3's: the walk forward, where a load reads the next length too

// [FW] a jump to an event's position: the anchor there, the jump's records (vc2JumpRecords); an unvoiced record clears
// the Energy filter's cutoff
void jumpTo(Vc2WalkWords& w, int32_t pos, uint32_t entry, uint32_t slot) {
    w.setWord(kAnchor, pos);
    vc2JumpRecords(w, kSoloRecords, pos, entry, slot);
    if ((w.half(kInfo) & kVoiced) == 0) w.setReal(kCutoff, 0.0f);
}

} // namespace

// [FW] 0x893c9b80: the next record (vc2RecordAfter), its length; at or before the range's end its budget and,
// walking forward, the next one's length; past it unlimited, the next length its own
int32_t loadNext(Vc2WalkWords& w) {
    const int32_t length = vc2RecordAfter(w, kSoloRecords);
    w.setWord(kRecLength, length);
    if (!(w.word(vc2walk::kRangeEnd) >= w.word(kRecEnd))) {
        w.setHalf(kBudget, vc2walk::kUnlimited);
        w.setWord(kNextLength, length);
        return vc2walk::kUnlimited;
    }
    w.setHalf(kBudget, vc2RecordBudget(w.half(kInfo)));
    uint32_t at = (uint32_t)w.word(kNextRecord);
    if ((w.byte(vc2walk::kWalkBits) & kForward) != 0) w.setWord(kNextLength, vc2RecordLength(w, at));
    return w.half(kBudget);
}

// [FW] 0x893c9bf0: the record before (vc2RecordBefore), its length; the next length the current's; the budget
int32_t loadPrev(Vc2WalkWords& w) {
    const int32_t next = w.word(kRecLength);
    w.setWord(kRecLength, vc2RecordBefore(w, kSoloRecords));
    w.setWord(kNextLength, next);
    return w.half(kBudget);
}

// [FW] SOLO's record block (Vc2OscSolo.h), its load of the next record
const Vc2WalkRecords kSoloRecords { kBudget,     kInfo,      kSegment,  kNextSegment, kEntryAfter,
                                    kNextFirst,  kNextRecord, kRecStart, kRecEnd,      loadNext };

// [FW] 0x893c9c60: the grains' parameters.  The period the record's length (×16, at least 160), and walking forward
// past the record's start a glide toward the next's by how far in the position is (past the end the next's, the
// second tap's weight 1); under Robot Voice the period's smoothed copy swings the rate.  The rate and the formant at
// least a floor; an unvoiced record keeps them only by its bits, and takes their product for both.  The trigger's
// period the period over the rate (at most a most) × 256; the anchor step the rate × 16 (at least 16); the grain's
// start offset what the formant's span leaves of the period; the windows' step 8 over that span; a voiced record's
// cutoff 16 over the trigger's span (at most ½)
void grainParams(Vc2WalkWords& w) {
    const Vc2OscSoloPool& k = vc2OscTables().solo;
    float period = flt(w.word(kRecLength)), next = flt(w.word(kNextLength));
    if (k.periodFloor > period) period = k.periodFloor;
    if (k.periodFloor > next) next = k.periodFloor;
    float tap = 0.0f;
    const int32_t pos = w.word(vc2walk::kPos), start = w.word(kRecStart);
    const bool forward = (w.byte(vc2walk::kWalkBits) & kForward) != 0;
    if (forward && pos > w.word(kRecEnd)) {
        tap = 1.0f;
        period = next;
    } else if (forward && pos > start) {
        tap = div(flt((int32_t)((uint32_t)pos - (uint32_t)start)), period);
        period = add(mul(tap, sub(next, period)), period);
    }
    w.setReal(kTap, tap);
    float rate = w.real(kRateNow);
    if ((w.byte(vc2walk::kFlags) & kRobot) != 0) {
        const float smooth = add(mul(k.robotGlide, sub(period, w.real(kSmooth))), w.real(kSmooth));
        w.setReal(kSmooth, smooth);
        const float most = mul(k.robotSwing, period);
        float swing = sub(smooth, period);
        if (swing > most) swing = most;   // FCMP/GT both ways: a NaN stays
        if (-most > swing) swing = -most;
        rate = mul(mul(rate, add(mul(w.real(kRobotDepth), swing), period)), k.robotScale);
    }
    float formant = w.real(kFormant);
    if (k.rateFloor > rate) rate = k.rateFloor;
    if (k.rateFloor > formant) formant = k.rateFloor;
    const int16_t info = w.half(kInfo);
    if ((info & kVoiced) == 0) {
        rate = mul((info & kKeepsPitch) != 0 ? rate : 1.0f, (info & kKeepsFormant) != 0 ? formant : 1.0f);
        formant = rate;
    }
    float ratio = div(period, rate);
    if (ratio > k.triggerMost) ratio = k.triggerMost;
    w.setReal(kWeight, formant);
    w.setReal(kRateTaken, rate);
    float step = mul(rate, k.stepScale);
    if (k.stepScale > step) step = k.stepScale;
    w.setWord(kTrigger, xtf::ftrc(mul(k.triggerScale, ratio)));
    w.setWord(kAnchorStep, xtf::ftrc(step));
    float span = div(period, formant);
    int32_t offset = 0;
    if (span > ratio) {
        offset = xtf::ftrc(sub(period, mul(formant, ratio)));
        span = ratio;
    }
    w.setWord(kOffset, offset);
    w.setReal(kWindowStep, div(k.windowSteps, span));
    if ((info & kVoiced) == 0) return;
    float cutoff = div(k.cutoffScale, ratio);
    if (cutoff > k.cutoffMost) cutoff = k.cutoffMost;
    w.setReal(kCutoff, cutoff);
}

// [FW] 0x893c9dd0: a repeat's cost: the anchor step (×1/16) on one of two lines (below 1, else), at least 0, × the
// walk's |step| (×2^−25) × 2^29, its top half word; below 0x800 doubled less 0x800, at least 0
int32_t repeatCost(const Vc2WalkWords& w) {
    const Vc2OscSoloPool& k = vc2OscTables().solo;
    const float a = mul(flt(w.word(kAnchorStep)), k.costAnchor);
    float walk = mul(flt(w.word(vc2walk::kStep)), k.costWalk);
    const bool low = 1.0f > a;
    float x = add(mul(a, low ? k.costLowSlope : k.costHighSlope), low ? k.costLowBase : k.costHighBase);
    if (0.0f > x) x = 0.0f;
    if (0.0f > walk) walk = -walk;
    constexpr uint32_t kTop = 16;
    constexpr int32_t kLeast = 0x800;
    int32_t c = (int32_t)((uint32_t)xtf::ftrc(mul(mul(x, walk), k.costScale)) >> kTop);
    if (kLeast > c) c = std::max(2 * c - kLeast, 0);
    return c;
}
} // namespace vc2solo

// [FW] SOLO's row: its two jumps through jumpTo (0x893c9350's and 0x893c9364's entries for indexes 4 and 8, 0x893c9e90
// and 0x893c9eb0), the step's sign negated at a turn, the start's seek and jump (0x8993fc00's and 0x8993fc14's entries
// 1 and 2: 0x893cc630, the voice's own, vc2OscSeekSolo, and 0x893c9e70, 0x893c9e90's wrapper)
const Vc2WalkWay kVc2SoloWalk { vc2RecordEvent<vc2solo::jumpTo>, vc2RecordCached<vc2solo::jumpTo>, false,
                                vc2RecordSeek<0x893cc630>, vc2RecordEvent<vc2solo::jumpTo> };
