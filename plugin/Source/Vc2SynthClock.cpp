// The part's clock (Vc2SynthClock.h, v354).
#include "Vc2SynthClock.h"
#include "XtTempoTicks.h"

namespace {
// [FW] row 20's object: its class, 20 ticks a clock, 500 ms a quarter (120 BPM), the units' rate, no clock seen, 20
// ticks to a clock out, 125 ms over six clocks
constexpr uint32_t kClockClass = 0x898c821cu;
constexpr int32_t kPerClock = 20, kQuarterMs = 500;
constexpr uint16_t kSpeed = 0x4000, kSix = 125;
constexpr int8_t kNoneSeen = -1, kOutCount = 20;
constexpr uint32_t kWaitEvent = 0x8000;            // [FW] the waiter's event (0x89313198)
constexpr uint16_t kSinceMost = 0xffff;            // [FW] the since count's end (0x893131aa)
constexpr uint32_t kNotesTask = 0x8994fec8u, kScheduleEvent = 0x10;   // [FW] the schedule's post (0x8938feac)
constexpr uint32_t kScheduleStep = 20;             // [FW] a MIDI clock's ticks, the schedule's step (0x8938feba)

XtTempoTicks ticksOf(const Vc2ClockObject& o) {
    XtTempoTicks t;
    t.speed = o.speed;
    t.units = o.units;
    t.unitFrac = o.unitFrac;
    t.unitsTaken = o.unitsTaken;
    t.remainder = o.remainder;
    t.ticks = o.ticks;
    t.fed = o.fed;
    t.ramp = o.ramp;
    return t;
}

void store(Vc2ClockObject& o, const XtTempoTicks& t) {
    o.units = t.units;
    o.unitFrac = t.unitFrac;
    o.unitsTaken = t.unitsTaken;
    o.remainder = t.remainder;
    o.ticks = t.ticks;
    o.fed = t.fed;
    o.ramp = t.ramp;
}
} // namespace

void Vc2SynthClock::build(Vc2ClockObject& o) {
    o = Vc2ClockObject {};
    o.klass = kClockClass;
    o.perClock = kPerClock;
    o.quarterMs = kQuarterMs;
    o.speed = kSpeed;
    o.seen = kNoneSeen;
    o.outCount = kOutCount;
    o.six = kSix;
}

void Vc2SynthClock::buildSchedule(Vc2ScheduleTimer& t) {   // 0x8930f000 on it, then its own class (0x89390418)
    t = Vc2ScheduleTimer {};
    t.timer.klass = Vc2ScheduleTimer::kClass;
}

Vc2TempoTimers Vc2SynthClock::timers() {
    return { list, [this](uint32_t) -> Vc2TempoTimer& { return schedule.timer; } };   // the schedule alone on the list
}

void Vc2SynthClock::clocksOut(uint32_t ticks) {
    Vc2ClockObject& o = object;
    if (ticks != 0 && o.sendOut != 0) {
        const int32_t left = o.outCount - (int32_t)ticks;
        o.outCount = (int8_t)(left > 0 ? left : left + kOutCount);
        o.outPending = (uint16_t)(o.outPending + (left > 0 ? 0 : 1));
    }
    if (o.outPending != 0 && parts.sendClock()) --o.outPending;
}

void Vc2SynthClock::ms() {
    Vc2ClockObject& o = object;
    XtTempoTicks t = ticksOf(o);
    if (o.sync == 0 && o.external != 0) {
        t.msExternal(o.six, o.since);   // 0x8931301c
    } else {
        const uint32_t before = t.ticks;
        t.msInternal(o.quarterMs);      // 0x89313080
        clocksOut(t.ticks - before);
    }
    store(o, t);
    if (o.waiter != 0 && o.waitUntil <= o.ticks) {   // 0x89313186
        parts.raise(o.waiter, kWaitEvent);
        o.waiter = 0;
    }
    if (o.seen >= 0) {   // 0x8931319e
        const uint16_t was = o.since;
        o.since = (uint16_t)(was + 1);
        if (was >= kSinceMost) {
            o.seen = kNoneSeen;
            o.since = 0;
        }
    }
    parts.raise(kTask, kTakeEvent);
}

void Vc2SynthClock::fired(uint32_t timer) {
    Vc2TempoTimers on = timers();
    if (on.timerAt(timer).klass != Vc2ScheduleTimer::kClass) return;
    ++schedule.count;
    parts.post(kNotesTask, kScheduleEvent);
    schedule.at += kScheduleStep;
    on.set(timer, schedule.at);
}

void Vc2SynthClock::pass(uint32_t events) {
    if ((events & kTakeEvent) == 0) return;
    XtTempoTicks t = ticksOf(object);
    const int32_t ticks = t.take();   // 0x89312fd0
    store(object, t);
    if (ticks != 0) timers().advance(ticks, [this](uint32_t timer) { fired(timer); });
}

void Vc2SynthClock::armSchedule() {
    schedule.count = 0;
    schedule.at = list.clock + kScheduleStep;
    timers().set(Vc2ScheduleTimer::kAt, schedule.at);
}
