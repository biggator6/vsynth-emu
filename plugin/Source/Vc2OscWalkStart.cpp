// The walk's start (v328): rom.4's 0x893cc1a0, SOLO's start, which mode 1's set-up runs with its own row, and the
// event lookup 0x893cc0d0 it reads by.  Every address is rom.4's, every offset the object's.
#include "Vc2OscWalkSteps.h"

namespace vc2walk {
namespace {
// [FW] the events' table's word holding the count of events, in its high half
constexpr uint32_t kCountAt = 8, kCountShift = 16;

// [FW] 0x893cc0d0: event n's kind and position, its entry (u8 stride × n + u8 first) half words into the table, as
// the lookup forms it (the bytes zero-extended, a whole product; the load's forms it otherwise past 0x7f)
Event lookup(const Vc2WalkWords& w, uint32_t n) {
    const uint32_t offset = 2 * ((uint32_t)w.byte(kStride) * n + w.byte(kFirst));
    return entryEvent(w, (uint32_t)w.word(kTable) + offset);
}
// An event's number as the start reads it, its half word zero-extended
uint16_t eventNumber(const Vc2WalkWords& w, uint32_t at) { return (uint16_t)w.half(at); }
} // namespace
} // namespace vc2walk

// [FW] 0x893cc1a0: the last event the table's count less 1, its position the last's (+0x20); then with the range in
// samples the event 0 and the player's seek to the range's start, or its end walking back; else the range's events
// held to the last, their positions its start and end, the current event the one the walk starts from, the position
// 0, and the player's jump there
void vc2WalkStart(Vc2OscVoice& v, Vc2OscParts& p, const Vc2WalkWay& way) {
    using namespace vc2walk;
    Vc2WalkWords w { v, p };
    const uint32_t count = w.memory((uint32_t)w.word(kTable) + kCountAt) >> kCountShift;
    w.setHalf(kLast, (int16_t)count - 1);
    w.setWord(kFinal, lookup(w, eventNumber(w, kLast)).pos);
    const bool back = 0.0f > w.real(kWalk);
    if ((w.byte(kFlags) & kInSamples) != 0) {
        w.setHalf(kIndex, 0);
        way.seekTo(v, p, w.word(back ? kRangeEnd : kRangeStart));
        return;
    }
    for (uint32_t at : { kRangeFirst, kRangeLast }) {
        if (!(eventNumber(w, kLast) >= eventNumber(w, at))) w.setHalf(at, w.half(kLast));
    }
    w.setWord(kRangeStart, lookup(w, eventNumber(w, kRangeFirst)).pos);
    w.setWord(kRangeEnd, lookup(w, eventNumber(w, kRangeLast)).pos);
    w.setHalf(kIndex, w.half(back ? kRangeLast : kRangeFirst));
    w.setWord(kPos, 0);
    way.startJump(v, p);
}
