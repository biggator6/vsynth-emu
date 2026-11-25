#pragma once
// rom.4's tempo timers (v354): the OS's list of timers on the tempo clock's ticks, 480 a quarter (rom.4's
// 0x8930f000…0x8930f1d8, rom.2's library at its own place). The clock task gives the list the ticks since its last
// take; each timer due by then fires through its class, which may set it again. On the part the list holds the notes'
// task's schedule alone, a timer each MIDI clock (HARDWARE_FIRMWARE.md, "The part's clock and tempo timers").
#include <cstdint>
#include <functional>

// A timer, 0x1c bytes as rom.4 lays it out (its constructor 0x8930f000)
struct Vc2TempoTimer {
    static constexpr uint32_t kOff = 0x7fffffffu;     // [FW] +8 off the list
    static constexpr uint32_t kFired = 0x7ffffffeu;   // [FW] +8 fired, still on the list until set again or removed
    uint32_t unread[2] {};       // +0, +4
    uint32_t time = kOff;        // +8: the tick it fires at
    uint32_t fired = 0;          // +0xc: the time it fired at, last
    uint32_t prev = 0, next = 0; // +0x10, +0x14: the list's links, the timers' addresses
    uint32_t klass = 0;          // +0x18: its class, whose method fires it
};
static_assert(sizeof(Vc2TempoTimer) == 0x1c);

// The notes' task's schedule (0x89981b40, class 0x898cd118, built by the part's first row, 0x893903b0): each firing
// counts one and posts the notes' task 0x10, then sets the timer 20 ticks on (0x8938fea0)
struct Vc2ScheduleTimer {
    static constexpr uint32_t kAt = 0x89981b40u, kClass = 0x898cd118u;   // [FW]
    static constexpr uint32_t kBaseClass = 0x898c7e5cu;                 // [FW] the timer's own (0x8930f012)
    Vc2TempoTimer timer {};
    uint32_t at = 0;             // +0x1c: its next time
    uint32_t count = 0;          // +0x20: its firings since the notes' task took them
};
static_assert(sizeof(Vc2ScheduleTimer) == 0x24);

// The list's words (0x8992e328…, .data)
struct Vc2TempoList {
    static constexpr uint32_t kAt = 0x8992e328u;   // [FW]
    uint32_t head = 0;                             // a timer's address, 0 for none
    uint32_t clock = 0;                            // 0x8992e32c: the ticks given so far
    uint32_t nextDue = Vc2TempoTimer::kOff;        // 0x8992e330: the soonest a timer on it is due
};
static_assert(sizeof(Vc2TempoList) == 12);

// The list's routines on its words, the timers found by their addresses (the remove, 0x8930f0d0, isn't the part's:
// nothing takes the schedule off)
struct Vc2TempoTimers {
    using TimerAt = std::function<Vc2TempoTimer&(uint32_t)>;
    using Fire = std::function<void(uint32_t)>;   // a due timer's class's method, by the timer's address
    Vc2TempoList& list;
    TimerAt timerAt;
    void set(uint32_t timer, uint32_t time);       // 0x8930f050: only sooner; on the list at its end if it was off
    void advance(int32_t ticks, const Fire& fire); // 0x8930f140: each timer due fired, the soonest kept
};
