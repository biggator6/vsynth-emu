// rom.4's tempo timers (Vc2TempoTimers.h, v354). Times compare signed, as rom.4's cmp/gt does; each routine masks the
// interrupts in rom.4, which here is nothing. The list is a ring (XtRing.h), the timers by their addresses.
#include "Vc2TempoTimers.h"
#include "XtRing.h"

namespace {
bool later(uint32_t a, uint32_t b) { return (int32_t)a > (int32_t)b; }
} // namespace

void Vc2TempoTimers::set(uint32_t timer, uint32_t time) {
    Vc2TempoTimer& t = timerAt(timer);
    if (!later(t.time, time)) return;
    if (t.time == Vc2TempoTimer::kOff) xtRingLink<uint32_t>(list.head, 0, timer, timerAt);   // 0x8930f07a
    t.time = time;
    if (later(list.nextDue, time)) list.nextDue = time;
}

void Vc2TempoTimers::advance(int32_t ticks, const Fire& fire) {
    list.clock += (uint32_t)ticks;
    if (later(list.nextDue, list.clock)) return;
    list.nextDue = Vc2TempoTimer::kOff;
    const uint32_t first = list.head;
    if (first == 0) return;
    uint32_t at = first;
    do {
        Vc2TempoTimer& t = timerAt(at);
        if (later(t.time, list.clock)) {
            if (later(list.nextDue, t.time)) list.nextDue = t.time;
        } else {
            t.fired = t.time;
            t.time = Vc2TempoTimer::kFired;
            fire(at);
        }
        at = timerAt(at).next;
    } while (at != first);
}
