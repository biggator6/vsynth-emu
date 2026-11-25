// The period-jump player's records, jumps, set-up and seek on the OSC voice (v335): rom.4's 0x893cabe0 and 0x893cac40
// (the next record and the one before, the record's length, its inverse and the next's as floats), the jumps
// 0x893cad20 and 0x893cad40 (the start's 0x893cacf0 the first's wrapper) and its row of the walk, its set-up 0x893ccff0,
// which VariPhrase's start runs for a wave of type 8, and its seek 0x893cd090; the record block's code the record
// players' (Vc2OscRecordBlock.cpp).  HARDWARE_FIRMWARE.md, "The VC-2's period-jump player".  Every address is rom.4's,
// every offset the object's.  Audio thread.
#include "Vc2OscJump.h"
#include "Vc2OscPlayers.h"
#include "Vc2OscTables.h"
#include "XtFloat.h"

namespace vc2jump {
namespace {
// [FW] the record's length into its float words: the length, its inverse
void lengthWords(Vc2WalkWords& w, int32_t length) {
    const float l = xtf::flt(length);
    w.setReal(kLength, l);
    w.setReal(kInverse, xtf::div(1.0f, l));
}

// [FW] a jump to an event's position: the read moved with the position (its fraction 0), the jump's records
// (vc2JumpRecords)
void jumpWithRead(Vc2WalkWords& w, int32_t pos, uint32_t entry, uint32_t slot) {
    w.setWord(kReadFrac, 0);
    w.setWord(kRead, vc2WalkPlus(vc2WalkMinus(w.word(kRead), w.word(vc2walk::kPos)), pos));
    vc2JumpRecords(w, kJumpRecords, pos, entry, slot);
}
} // namespace

// [FW] 0x893cabe0: the next record (vc2RecordAfter), its length and inverse; at or before the last event's position
// its budget and the next one's length; past it unlimited, the next length its own
int32_t loadNextFloat(Vc2WalkWords& w) {
    lengthWords(w, vc2RecordAfter(w, kJumpRecords));
    if (!(w.word(vc2walk::kFinal) >= w.word(kRecEnd))) {
        w.setReal(kNextLength, w.real(kLength));
        w.setHalf(kBudget, vc2walk::kUnlimited);
        return vc2walk::kUnlimited;
    }
    uint32_t at = (uint32_t)w.word(kNextRecord);
    w.setReal(kNextLength, xtf::flt(vc2RecordLength(w, at)));
    w.setHalf(kBudget, vc2RecordBudget(w.half(kInfo)));
    return w.half(kBudget);
}

// [FW] 0x893cac40: the record before (vc2RecordBefore), its length and inverse; the next length the current's
void loadPrevFloat(Vc2WalkWords& w) {
    const float next = w.real(kLength);
    lengthWords(w, vc2RecordBefore(w, kJumpRecords));
    w.setReal(kNextLength, next);
}

// [FW] the period-jump player's record block (Vc2OscJump.h), its load of the next record
const Vc2WalkRecords kJumpRecords { kBudget,    kInfo,       kSegment,  kNextSegment, kEntryAfter,
                                    kNextFirst, kNextRecord, kRecStart, kRecEnd,      loadNextFloat };
} // namespace vc2jump

// [FW] the period-jump player's row: its two jumps through jumpWithRead (0x893c9350's and 0x893c9364's entries for
// index 12, 0x893cad20 and 0x893cad40), the step's sign negated at a turn, the start's seek and jump (0x8993fc00's and
// 0x8993fc14's entries 3: 0x893cd090, the voice's own, vc2OscSeekJump, and 0x893cacf0, 0x893cad20's wrapper)
const Vc2WalkWay kVc2JumpWalk { vc2RecordEvent<vc2jump::jumpWithRead>, vc2RecordCached<vc2jump::jumpWithRead>, false,
                                vc2RecordSeek<0x893cd090>, vc2RecordEvent<vc2jump::jumpWithRead> };

// [FW] 0x893ccff0: byte 3 all but its 0x40 cleared, the end 0, the gain's rate 1/64, the samples from the head, the
// position and the read 0; the table, cached events, layout and walk's start with the jump's row (vc2WalkSetUp, in
// SOLO's layout); the loops' rates (0x893ccf50) and the beat's start (0x893cbf40)
void vc2OscJumpSetUp(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared) {
    using namespace vc2jump;
    using namespace vc2walk;
    constexpr uint32_t kSamplesFrom = 0x4;   // [FW] the head's +4, the samples
    Vc2WalkWords w { v, p };
    w.setByte(kWalkBits, (uint8_t)(w.byte(kWalkBits) & kHeld));
    w.setByte(kEnded, 0);
    w.setReal(kGainRate, vc2OscTables().jump.setUpGain);
    w.setWord(kSamples, (int32_t)vc2OscHeadWord(v, p, kSamplesFrom));
    w.setWord(kPos, 0);
    w.setWord(kRead, 0);
    vc2WalkSetUp(v, p, kVc2SoloLayout, kVc2JumpWalk);
    vc2OscLoopRates(v, p, shared);
    v.beatStart();
}

// [FW] 0x893cd090: the walk's seek with the jump's row (vc2WalkSeek); then in samples the records' walk forward to the
// position (vc2SeekRecords).  The beat's reset (0x893cbfe0) comes before the records' walk in rom.4, which reads and
// writes nothing of it; the voice's seekBy resets it after
void vc2OscSeekJump(Vc2OscVoice& v, Vc2OscParts& p, int32_t at) {
    Vc2WalkWords w { v, p };
    const bool inSamples = (w.byte(vc2walk::kFlags) & vc2walk::kInSamples) != 0;
    vc2WalkSeek(v, p, at, kVc2JumpWalk);
    if (inSamples) vc2SeekRecords(w, vc2jump::kJumpRecords);
}
