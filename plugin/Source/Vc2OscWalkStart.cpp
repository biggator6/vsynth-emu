// The walk's start (v328): rom.4's 0x893cc1a0, SOLO's start, which mode 1's set-up runs with its own row, and the
// event lookup 0x893cc0d0 it reads by; from v329 mode 1's seek 0x893cc830 and the find 0x893cc100 it runs.  Every
// address is rom.4's, every offset the object's.
#include "Vc2OscWalkSteps.h"
#include <algorithm>

namespace vc2walk {
namespace {
// [FW] the events' table's word holding the count of events, in its high half
constexpr uint32_t kCountAt = 8, kCountShift = 16;
// [FW] v329: the beat's position (+0x64), which the seek sets to the position (+0x38) × 32
constexpr uint32_t kBeatPos = 0x64, kBeatShift = 5;

// [FW] 0x893cc0d0: event n's kind and position, its entry (u8 stride × n + u8 first) half words into the table, as
// the lookup forms it (the bytes zero-extended, a whole product; the load's forms it otherwise past 0x7f)
Event lookup(const Vc2WalkWords& w, uint32_t n) {
    const uint32_t offset = 2 * ((uint32_t)w.byte(kStride) * n + w.byte(kFirst));
    return entryEvent(w, (uint32_t)w.word(kTable) + offset);
}
// An event's number as the start reads it, its half word zero-extended
uint16_t eventNumber(const Vc2WalkWords& w, uint32_t at) { return (uint16_t)w.half(at); }

// [FW] 0x893cc100 (v329): the current event (+0xc) moved by the positions the lookup gives.  Below `at`, forward while
// the next is below it, ending at the one before the first that isn't (at the last event, the one before it); above
// it, back while it is above, down to the event 0; at it, where it is.  The numbers compared unsigned.
void find(Vc2WalkWords& w, int32_t at) {
    if (lookup(w, eventNumber(w, kIndex)).pos >= at) {
        for (uint16_t n = eventNumber(w, kIndex); at < lookup(w, n).pos && n > 0; n = eventNumber(w, kIndex)) {
            w.setHalf(kIndex, n - 1);
        }
        return;
    }
    for (;;) {
        const uint16_t n = eventNumber(w, kIndex);
        if (n >= eventNumber(w, kLast)) {
            w.setHalf(kIndex, n - 1);
            return;
        }
        w.setHalf(kIndex, n + 1);
        if (lookup(w, (uint16_t)(n + 1)).pos >= at) {
            w.setHalf(kIndex, n);
            return;
        }
    }
}
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

// [FW] v331: 0x893cc0d0's position (×16) of event `event`, as VariPhrase's note-on reads its range by events
int32_t vc2OscEventPosition(Vc2OscVoice& v, Vc2OscParts& p, uint32_t event) {
    return vc2walk::lookup(Vc2WalkWords { v, p }, event).pos;
}

// [FW] 0x893cc830 (v329): with the range in samples, `at` held to 0…the last event's position, the event found for
// it, the jump there (0x893ca160), and the position `at`; else the current event `at`'s low half, held to the last
// (unsigned), and the jump; then the beat's position the position × 32.  The beat's reset after it is the voice's
// (Vc2OscVoice::seekBy).
void vc2OscSeekModeOne(Vc2OscVoice& v, Vc2OscParts& p, int32_t at) {
    using namespace vc2walk;
    Vc2WalkWords w { v, p };
    if ((w.byte(kFlags) & kInSamples) != 0) {
        const int32_t held = std::min(std::max(at, 0), w.word(kFinal));
        find(w, held);
        kVc2ModeOneWalk.startJump(v, p);
        w.setWord(kPos, held);
    } else {
        w.setHalf(kIndex, at);
        if (!(eventNumber(w, kLast) >= (uint16_t)at)) w.setHalf(kIndex, w.half(kLast));
        kVc2ModeOneWalk.startJump(v, p);
    }
    w.setWord(kBeatPos, (int32_t)((uint32_t)w.word(kPos) << kBeatShift));
}
