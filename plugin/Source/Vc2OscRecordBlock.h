#pragma once
// The record players' record block (v335): the words SOLO (VariPhrase's types 1 and 6, Vc2OscSolo.h) and the
// period-jump player (type 8, Vc2OscJump.h) keep on a wave's records past the samples' address (+0xec), SOLO's four
// bytes up past its grain toggle, and the routines each has of its own in rom.4 that are one code on that layout: a
// record's whole length, the record before, a segment entry, a jump's records, a seek's walk forward.  The records are
// the vpdt's: a length (×16) and an info word, 0xffff continuing a length.  HARDWARE_FIRMWARE.md, "The VC-2's
// period-jump player".  Every address is rom.4's, every offset the object's.  Audio thread.
#include "Vc2OscWalk.h"

// A player's record block: its repeat budget and record's info (half words), the segment's start and the next's, the
// entry after the next, the next segment's first record, the next record, the record's start and end (×16); its own
// load of the next record, which gives the budget
struct Vc2WalkRecords {
    uint32_t budget, info, segment, nextSegment, entryAfter, nextFirst, nextRecord, recStart, recEnd;
    int32_t (*loadNext)(Vc2WalkWords& w);
};

namespace vc2walk {
// [FW] the budget's bits in an info word (<< 6 the budget), all of them unlimited
inline constexpr int16_t kBudgetBits = 0x1f8, kUnlimited = 0x7e00;
inline constexpr int kBudgetShift = 6;
} // namespace vc2walk

// one-off: the routines' declarations, alike only in taking the words and a layout
int32_t vc2RecordLength(const Vc2WalkWords& w, uint32_t& at);   // a record's whole length; `at` past its last entry
int32_t vc2RecordBudget(int32_t info);                         // an info word's budget
int32_t vc2RecordAfter(Vc2WalkWords& w, const Vc2WalkRecords& r);    // 0x893c9b80's, 0x893cabe0's walk: its length
int32_t vc2RecordBefore(Vc2WalkWords& w, const Vc2WalkRecords& r);   // 0x893c9bf0, 0x893cac40: its length
int32_t vc2SegmentEntry(Vc2WalkWords& w, const Vc2WalkRecords& r, uint32_t at);   // 0x893c9e50, 0x893cacd0
void vc2JumpRecords(Vc2WalkWords& w, const Vc2WalkRecords& r, int32_t pos, uint32_t entry, uint32_t slot);
void vc2SeekRecords(Vc2WalkWords& w, const Vc2WalkRecords& r);   // 0x893cc630's, 0x893cd090's walk in samples

// [FW] the record players' two jumps of the walk's row, each through the player's own jump to a position (its
// entry, its slot of a segment entry, a record and a mark): to the current event, its slot past its entry (SOLO's
// 0x893c9e90, the period-jump player's 0x893cad20); to a loop's cached event, its number the current event's, then its
// position and slot (0x893c9eb0, 0x893cad40)
using Vc2RecordJump = void (*)(Vc2WalkWords& w, int32_t pos, uint32_t entry, uint32_t slot);
void vc2JumpToEvent(Vc2OscVoice& v, Vc2OscParts& p, Vc2RecordJump to);
void vc2JumpToCached(Vc2OscVoice& v, Vc2OscParts& p, uint32_t cached, Vc2RecordJump to);
// A record player's ways for its walk row (Vc2WalkWay): the two jumps through its own `To`, the start's seek its own
// routine `Seek` by the voice's seekBy
template <Vc2RecordJump To>
void vc2RecordEvent(Vc2OscVoice& v, Vc2OscParts& p) { vc2JumpToEvent(v, p, To); }
template <Vc2RecordJump To>
void vc2RecordCached(Vc2OscVoice& v, Vc2OscParts& p, uint32_t cached) { vc2JumpToCached(v, p, cached, To); }
template <uint32_t Seek>
void vc2RecordSeek(Vc2OscVoice& v, Vc2OscParts& p, int32_t at) { v.seekBy(Seek, at, p); }
