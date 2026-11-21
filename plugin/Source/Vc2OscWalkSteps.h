#pragma once
// What the walk's two files share (v327): the object's offsets and bits, an event's entry and its load, the next and
// the previous event, the walk's bits; and the events' ways at a crossing (Vc2OscWalkEvents.cpp).  Every address is
// rom.4's, every offset the object's (from the voice's +0x15c).
#include "Vc2OscWalk.h"

namespace vc2walk {
// [FW] the object's bytes: its flags (0x01 the loops on); the events' bits, byte 2 (0x01 an event of kind 2 or 3 acts,
// 0x02 the next such passes and is cleared, 0x04 a jump or turn sets the walk's 0x01); the walk's, byte 3 (0x01 a
// sample's step, 0x02 turned by an event, 0x04 bouncing in the backward loop, walking forward from its start, 0x08 in
// the forward loop, walking back from its end, 0x10 in the backward loop, 0x20 in the forward loop, 0x40 held); the
// end (1 past the last event, −1 before the first, 0 looping); the current and the next event's kinds; the events'
// first half word and stride
inline constexpr uint32_t kFlags = 0x0, kEventBits = 0x2, kWalkBits = 0x3, kEnded = 0x4, kKind = 0x5;
inline constexpr uint32_t kNextKind = 0x6, kFirst = 0x7, kStride = 0x8;
// [FW] its words: the current and the last event's numbers (half words), the events' table, the last event's
// position, the current and the next event's, the next crossing, the step, the position (×16), its fraction, the
// loops' record, the rate factor and the forward and backward loops'
inline constexpr uint32_t kIndex = 0xc, kLast = 0xe, kTable = 0x10, kFinal = 0x20, kPrev = 0x24, kNext = 0x28;
inline constexpr uint32_t kCross = 0x2c, kStep = 0x34, kPos = 0x38, kFrac = 0x3c, kRecord = 0x6c, kRate = 0xa0;
inline constexpr uint32_t kFwdRate = 0xa4, kBackRate = 0xa8;
inline constexpr uint8_t kLoopsOn = 0x01, kActs = 0x01, kPasses = 0x02, kSets = 0x04, kMarkBits = kActs | kSets;
inline constexpr uint8_t kOneSample = 0x01, kTurned = 0x02, kBounceBack = 0x04, kBounceFwd = 0x08;
inline constexpr uint8_t kInBack = 0x10, kInFwd = 0x20, kHeld = 0x40, kBefore = 0xff;
// [FW] the three kinds of event (an entry's flags & 0x0b) the walk acts on
inline constexpr uint8_t kMark = 1, kReturn = 2, kTurn = 3;

struct Event {
    uint8_t kind;
    int32_t pos;
};
uint32_t offsetOf(const Vc2WalkWords& w, int32_t number);   // an event's entry from the table, by its number
Event entryEvent(const Vc2WalkWords& w, uint32_t entry);   // its kind and position
uint32_t load(Vc2WalkWords& w);       // 0x893c8f00
void readNext(Vc2WalkWords& w);       // 0x893c8f30, as its callers store it
void advance(Vc2WalkWords& w);        // 0x893c9100
void retreat(Vc2WalkWords& w);        // 0x893c9300
void setWalk(Vc2WalkWords& w, uint8_t bits);
void clearWalk(Vc2WalkWords& w, uint8_t bits);
void negateStep(Vc2WalkWords& w, const Vc2WalkWay& way);
void forwardEvent(Vc2WalkWords& w, const Vc2WalkWay& way);    // 0x893c9040…, at the next event
void backwardEvent(Vc2WalkWords& w, const Vc2WalkWay& way);   // 0x893c9230…, at the current event
} // namespace vc2walk
