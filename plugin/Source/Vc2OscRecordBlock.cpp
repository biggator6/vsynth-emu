// The record players' shared record code (v335, from v334's SOLO): rom.4's SOLO and period-jump player each have
// these routines of their own, one code on the record block's layout (Vc2OscRecordBlock.h): the next record's walk
// (0x893c9b80's, 0x893cabe0's), the record before (0x893c9bf0, 0x893cac40), a segment entry (0x893c9e50, 0x893cacd0),
// a jump's records (0x893c9e90's and 0x893c9eb0's, 0x893cad20's and 0x893cad40's), a seek's walk forward in samples
// (0x893cc630's, 0x893cd090's).  HARDWARE_FIRMWARE.md, "The VC-2's period-jump player".  Every address is rom.4's,
// every offset the object's.  Audio thread.
#include "Vc2OscRecordBlock.h"
#include "Vc2OscWalkSteps.h"

namespace {
constexpr uint32_t kRecordBytes = 4, kInfoAt = 2, kOnward = 0xffff;   // [FW] a record; its info; a length going on
constexpr uint32_t kEntryBytes = 12, kStartAt = 8;   // [FW] a segment entry: its first record (bit 0 a flag), its start

uint16_t lengthAt(const Vc2WalkWords& w, uint32_t at) { return (uint16_t)w.memoryHalf(at); }
} // namespace

int32_t vc2RecordLength(const Vc2WalkWords& w, uint32_t& at) {
    int32_t length = 0;
    uint16_t part = kOnward;
    while (part == kOnward) {
        part = lengthAt(w, at);
        length = vc2WalkPlus(length, part);
        at += kRecordBytes;
    }
    return length;
}

int32_t vc2RecordBudget(int32_t info) {
    return (int32_t)((uint32_t)(info & vc2walk::kBudgetBits) << vc2walk::kBudgetShift);
}

// [FW] the next record from the next record's address: its length, its info (its last entry's), its start the
// record's end, its end that and its length
int32_t vc2RecordAfter(Vc2WalkWords& w, const Vc2WalkRecords& r) {
    uint32_t at = (uint32_t)w.word(r.nextRecord);
    const int32_t length = vc2RecordLength(w, at);
    w.setWord(r.nextRecord, (int32_t)at);
    w.setHalf(r.info, w.memoryHalf(at - kRecordBytes + kInfoAt));
    const int32_t start = w.word(r.recEnd);
    w.setWord(r.recStart, start);
    w.setWord(r.recEnd, vc2WalkPlus(start, length));
    return length;
}

// [FW] the record before: back from the next record's address over the current record's entries to the last entry of
// the one before (the next record's address then its end), its whole length back over its going-on entries; its end
// the current's start, its start that less its length (at 0 at the least, its end then its length); its info and
// budget
int32_t vc2RecordBefore(Vc2WalkWords& w, const Vc2WalkRecords& r) {
    constexpr uint32_t kBack = 2 * kRecordBytes;
    uint32_t at = (uint32_t)w.word(r.nextRecord);
    uint16_t part = kOnward;
    int16_t info = 0;
    while (part == kOnward) {
        at -= kBack;
        part = lengthAt(w, at);
        info = w.memoryHalf(at + kInfoAt);
        at += kRecordBytes;
    }
    w.setWord(r.nextRecord, (int32_t)at);
    int32_t length = part;
    for (uint32_t before = at - kBack; lengthAt(w, before) == kOnward; before -= kRecordBytes) {
        length = vc2WalkPlus(length, (int32_t)kOnward);
    }
    const int32_t end = w.word(r.recStart);
    w.setWord(r.recEnd, end);
    int32_t start = end - length;
    if (!(start >= 0)) {
        start = 0;
        w.setWord(r.recEnd, length);
    }
    w.setWord(r.recStart, start);
    w.setHalf(r.info, info);
    w.setHalf(r.budget, vc2RecordBudget(info));
    return length;
}

// [FW] a segment entry at `at`: its first record (bit 0 cleared) into the table as the next segment's, its start the
// next segment's, the entry after it; its start
int32_t vc2SegmentEntry(Vc2WalkWords& w, const Vc2WalkRecords& r, uint32_t at) {
    const uint32_t first = (w.memory(at) & ~1u) + (uint32_t)w.word(vc2walk::kTable);
    const int32_t start = (int32_t)w.memory(at + kStartAt);
    w.setWord(r.entryAfter, (int32_t)(at + kEntryBytes));
    w.setWord(r.nextFirst, (int32_t)first);
    w.setWord(r.nextSegment, start);
    return start;
}

// [FW] a jump to an event's position (an event's entry, or a loop's cached event): the walk there, the next event read
// from the entry; from the slot (the entry's +8, the cached event's) the segment entry and the record, the segment
// and the next; the record's end the position + the mark − the record's first length, then the record loaded from it
void vc2JumpRecords(Vc2WalkWords& w, const Vc2WalkRecords& r, int32_t pos, uint32_t entry, uint32_t slot) {
    vc2walk::jumpTail(w, pos, entry);
    const uint32_t table = (uint32_t)w.word(vc2walk::kTable);
    const uint32_t segment = w.memory(slot) + table, record = w.memory(slot + 4) + table;
    const uint16_t mark = (uint16_t)w.memoryHalf(slot + 8);
    w.setWord(r.entryAfter, (int32_t)segment);
    w.setWord(r.nextRecord, (int32_t)record);
    const int32_t start = vc2SegmentEntry(w, r, segment);
    vc2SegmentEntry(w, r, (uint32_t)w.word(r.entryAfter));
    w.setWord(r.segment, start);
    w.setWord(r.recEnd, vc2WalkPlus(pos, (int32_t)mark - (int32_t)lengthAt(w, record)));
    r.loadNext(w);
}

void vc2JumpToEvent(Vc2OscVoice& v, Vc2OscParts& p, Vc2RecordJump to) {
    constexpr uint32_t kSlotAt = 8;   // [FW] an entry's segment entry, record and mark
    Vc2WalkWords w { v, p };
    const uint32_t entry = vc2walk::load(w);
    to(w, w.word(vc2walk::kPrev), entry, entry + kSlotAt);
}

void vc2JumpToCached(Vc2OscVoice& v, Vc2OscParts& p, uint32_t cached, Vc2RecordJump to) {
    constexpr uint32_t kPosAt = 4, kSlotAt = 8;   // [FW] a cached event's
    Vc2WalkWords w { v, p };
    w.setHalf(vc2walk::kIndex, w.memoryHalf(cached));
    const uint32_t entry = vc2walk::load(w);
    to(w, (int32_t)w.memory(cached + kPosAt), entry, cached + kSlotAt);
}

// [FW] after the walk's seek in samples: the segments forward while the next starts at or before the position and
// before the last event, each with its first record, and the records forward while their end is at or before it
void vc2SeekRecords(Vc2WalkWords& w, const Vc2WalkRecords& r) {
    const int32_t to = w.word(vc2walk::kPos);
    while (!(w.word(r.nextSegment) > to) && !(w.word(r.nextSegment) >= w.word(vc2walk::kFinal))) {
        const int32_t next = w.word(r.nextSegment);
        w.setWord(r.segment, next);
        w.setWord(r.recEnd, next);
        w.setWord(r.nextRecord, w.word(r.nextFirst));
        vc2SegmentEntry(w, r, (uint32_t)w.word(r.entryAfter));
        r.loadNext(w);
    }
    while (!(w.word(r.recEnd) > to)) r.loadNext(w);
}
