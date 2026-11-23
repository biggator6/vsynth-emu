// SOLO's records and grains' parameters on the OSC voice (v334): rom.4's 0x893c9b80 and 0x893c9bf0 (rom.2's 0x8950d4c0,
// 0x8950d530), 0x893c9e50 (0x8950d770), 0x893c9c60 (0x8950d5a0), 0x893c9dd0 (0x8950d6f0) and the jumps 0x893c9e90 and
// 0x893c9eb0 (0x8950d790, 0x8950d7d0), SOLO's row of the walk (Vc2OscSolo.h).  The records are the vpdt's: a length
// (×16) and an info word, 0xffff continuing a length.  HARDWARE_FIRMWARE.md, "The VC-2's SOLO".  Every address is
// rom.4's, every offset the object's.  Audio thread.
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
constexpr uint32_t kRecordBytes = 4, kInfoAt = 2, kOnward = 0xffff;   // [FW] a record; its info; a length going on
constexpr uint32_t kEntryBytes = 12, kStartAt = 8;   // [FW] a segment entry: its first record (bit 0 a flag), its start
constexpr uint8_t kForward = 0x80;   // [FW] byte 3's: the walk forward, where a load reads the next length too

uint16_t lengthAt(const Vc2WalkWords& w, uint32_t at) { return (uint16_t)w.memoryHalf(at); }
// The budget from an info word: its bits << 6
int32_t budgetOf(int32_t info) { return (int32_t)((uint32_t)(info & kBudgetBits) << kBudgetShift); }

// A record's whole length from its first entry, `at` past its last
int32_t lengthFrom(const Vc2WalkWords& w, uint32_t& at) {
    int32_t length = 0;
    uint16_t part = kOnward;
    while (part == kOnward) {
        part = lengthAt(w, at);
        length = vc2WalkPlus(length, part);
        at += kRecordBytes;
    }
    return length;
}

// [FW] a jump to an event's position (an event's entry, or a loop's cached event): the anchor and the walk there, the
// next event read from the entry; from the slot (the entry's +8, the cached event's) the segment entry and the record,
// the segment and the next; the record's end the position + the mark − the record's first length, then the record
// loaded from it; an unvoiced record clears the Energy filter's cutoff
void jumpTo(Vc2WalkWords& w, int32_t pos, uint32_t entry, uint32_t slot) {
    w.setWord(kAnchor, pos);
    vc2walk::jumpTail(w, pos, entry);
    const uint32_t table = (uint32_t)w.word(vc2walk::kTable);
    const uint32_t segment = w.memory(slot) + table, record = w.memory(slot + 4) + table;
    const uint16_t mark = (uint16_t)w.memoryHalf(slot + 8);
    w.setWord(kEntryAfter, (int32_t)segment);
    w.setWord(kNextRecord, (int32_t)record);
    const int32_t start = segmentEntry(w, segment);
    segmentEntry(w, (uint32_t)w.word(kEntryAfter));
    w.setWord(kSegment, start);
    w.setWord(kRecEnd, vc2WalkPlus(pos, (int32_t)mark - (int32_t)lengthAt(w, record)));
    loadNext(w);
    if ((w.half(kInfo) & kVoiced) == 0) w.setReal(kCutoff, 0.0f);
}

// [FW] 0x893c9e90 (the walk's for indexes 4 and 8, the start's 0x893c9e70 its wrapper): to the current event
void soloJump(Vc2OscVoice& v, Vc2OscParts& p) {
    Vc2WalkWords w { v, p };
    const uint32_t entry = vc2walk::load(w);
    constexpr uint32_t kSlotAt = 8;   // [FW] an entry's segment entry, record and mark
    jumpTo(w, w.word(vc2walk::kPrev), entry, entry + kSlotAt);
}
// [FW] 0x893c9eb0: to a loop's cached event: its number the current event's, then its position, segment entry, record
// and mark
void soloCached(Vc2OscVoice& v, Vc2OscParts& p, uint32_t cached) {
    Vc2WalkWords w { v, p };
    constexpr uint32_t kPosAt = 4, kSlotAt = 8;   // [FW] a cached event's
    w.setHalf(vc2walk::kIndex, w.memoryHalf(cached));
    const uint32_t entry = vc2walk::load(w);
    jumpTo(w, (int32_t)w.memory(cached + kPosAt), entry, cached + kSlotAt);
}
// [FW] the start's seek for indexes 1 and 2 (the table at 0x8993fc00), 0x893cc630, the voice's own (vc2OscSeekSolo)
void soloSeek(Vc2OscVoice& v, Vc2OscParts& p, int32_t at) {
    constexpr uint32_t kSoloSeek = 0x893cc630;
    v.seekBy(kSoloSeek, at, p);
}
} // namespace

// [FW] 0x893c9b80: the next record, from the next record's address: its length, info, start (the record's end) and end;
// at or before the range's end its budget and, walking forward, the next one's length; past it unlimited, the next
// length its own
int32_t loadNext(Vc2WalkWords& w) {
    uint32_t at = (uint32_t)w.word(kNextRecord);
    const int32_t length = lengthFrom(w, at);
    const int16_t info = w.memoryHalf(at - kRecordBytes + kInfoAt);
    w.setWord(kNextRecord, (int32_t)at);
    w.setHalf(kInfo, info);
    w.setWord(kRecLength, length);
    const int32_t start = w.word(kRecEnd);
    w.setWord(kRecStart, start);
    w.setWord(kRecEnd, vc2WalkPlus(start, length));
    if (!(w.word(vc2walk::kRangeEnd) >= w.word(kRecEnd))) {
        w.setHalf(kBudget, kUnlimited);
        w.setWord(kNextLength, length);
        return kUnlimited;
    }
    w.setHalf(kBudget, budgetOf(info));
    if ((w.byte(vc2walk::kWalkBits) & kForward) != 0) w.setWord(kNextLength, lengthFrom(w, at));
    return w.half(kBudget);
}

// [FW] 0x893c9bf0: the record before: back from the next record's address over the current record's entries to the
// last entry of the one before (the next record's address then its end), its whole length back over its going-on
// entries; its end the current's start, its start that less its length (at 0 at the least, its end then its length);
// the next length the current's; its info and budget
int32_t loadPrev(Vc2WalkWords& w) {
    constexpr uint32_t kBack = 2 * kRecordBytes;
    uint32_t at = (uint32_t)w.word(kNextRecord);
    uint16_t part = kOnward;
    int16_t info = 0;
    while (part == kOnward) {
        at -= kBack;
        part = lengthAt(w, at);
        info = w.memoryHalf(at + kInfoAt);
        at += kRecordBytes;
    }
    w.setWord(kNextRecord, (int32_t)at);
    int32_t length = part;
    for (uint32_t before = at - kBack; lengthAt(w, before) == kOnward; before -= kRecordBytes) {
        length = vc2WalkPlus(length, (int32_t)kOnward);
    }
    const int32_t end = w.word(kRecStart), next = w.word(kRecLength);
    w.setWord(kRecLength, length);
    w.setWord(kRecEnd, end);
    w.setWord(kNextLength, next);
    int32_t start = end - length;
    if (!(start >= 0)) {
        start = 0;
        w.setWord(kRecEnd, length);
    }
    w.setWord(kRecStart, start);
    w.setHalf(kInfo, info);
    w.setHalf(kBudget, budgetOf(info));
    return budgetOf(info);
}

// [FW] 0x893c9e50: a segment entry at `at`: its first record (bit 0 cleared) into the table as the next segment's, its
// start the next segment's, the entry after it; its start
int32_t segmentEntry(Vc2WalkWords& w, uint32_t at) {
    const uint32_t first = (w.memory(at) & ~1u) + (uint32_t)w.word(vc2walk::kTable);
    const int32_t start = (int32_t)w.memory(at + kStartAt);
    w.setWord(kEntryAfter, (int32_t)(at + kEntryBytes));
    w.setWord(kNextFirst, (int32_t)first);
    w.setWord(kNextSegment, start);
    return start;
}

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

// [FW] SOLO's row: its two jumps (0x893c9350's and 0x893c9364's entries for indexes 4 and 8), the step's sign negated at
// a turn, the start's seek and jump (0x8993fc00's and 0x8993fc14's entries 1 and 2)
const Vc2WalkWay kVc2SoloWalk { vc2solo::soloJump, vc2solo::soloCached, false, vc2solo::soloSeek, vc2solo::soloJump };
