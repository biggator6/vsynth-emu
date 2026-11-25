#pragma once
// The part's clock (v354): rom.4's tempo clock object (0x8995cb40) as its static initializer row 20 (0x893133d0)
// builds it, its millisecond (0x89313000, TMU0's handler's on each OS tick; rom.2's 0x894710e0, the same code), and
// the clock task's pass (0x89313c00's loop from 0x89313c36): its take (0x89312fd0) gives the tempo timers the ticks
// since the last, and the notes' schedule fires on them. The counting is XtTempoTicks', on the object's words.
// HARDWARE_FIRMWARE.md, "The part's clock and tempo timers".
#include "Vc2OsQueue.h"   // v355: Vc2RingNode
#include "Vc2TempoTimers.h"
#include <cstdint>

// The clock object, 0x50 bytes as rom.4 lays it out; its +0x4c, the clocks sent out's switch, read past its fields
struct Vc2ClockObject {
    Vc2RingNode node {};         // +0, +4 (v355): its node on the MIDI IN ring, while its clocks come from outside
    uint32_t klass = 0;          // +8
    uint16_t unread0c = 0;       // +0xc
    uint16_t sync = 0;           // +0xe: set, the internal count, whatever +0x40 says
    int32_t perClock = 0;        // +0x10: 20 ticks a clock
    int32_t quarterMs = 0;       // +0x14: a quarter's ms
    uint16_t unread18 = 0;       // +0x18
    uint16_t pad1a = 0;
    int32_t units = 0;           // +0x1c: 24 a ms
    uint16_t unitFrac = 0;       // +0x20
    uint16_t pad22 = 0;
    int32_t unitsTaken = 0;      // +0x24
    uint32_t ticks = 0;          // +0x28
    uint16_t remainder = 0;      // +0x2c
    uint16_t pad2e = 0;
    uint32_t waitUntil = 0;      // +0x30: the tick a waiting task is woken at
    uint32_t fed = 0;            // +0x34: ticks given to the tempo timers
    uint16_t speed = 0;          // +0x38: the units' rate, 16384ths
    uint16_t pad3a = 0;
    uint32_t waiter = 0;         // +0x3c: a task waiting for +0x30's tick (event 0x8000), 0 for none
    uint8_t external = 0;        // +0x40: the clocks from outside
    int8_t seen = 0;             // +0x41: clocks into the count, -1 before the first
    int8_t outCount = 0;         // +0x42: ticks to the next clock sent out
    uint8_t pad43 = 0;
    uint16_t outPending = 0;     // +0x44: clocks to send out
    uint16_t since = 0;          // +0x46: ms since the last clock from outside
    uint16_t six = 0;            // +0x48: the ms over the last six
    uint16_t ramp = 0;           // +0x4a: ticks past the last, in 256ths
    uint16_t sendOut = 0;        // +0x4c
    uint16_t pad4e = 0;
};
static_assert(sizeof(Vc2ClockObject) == 0x50);

// What the clock reaches of the part: the OS's events, and the MIDI clock's way out
struct Vc2ClockParts {
    virtual ~Vc2ClockParts() = default;
    virtual void raise(uint32_t task, uint32_t flags) = 0;   // an event set from an interrupt (0x89301840)
    virtual void post(uint32_t task, uint32_t flags) = 0;    // an event set from a task (0x89301830)
    virtual bool sendClock() = 0;                            // a clock out (0x89311780(0xf8)): whether it went
    // v355: an ID's value as rom.4's getter (0x89324350) answers it, on System B's block (0x89320760) or part 0's
    // (0x893207b0)
    virtual int32_t systemBValue(int16_t id) = 0;
    virtual int32_t patchValue(int16_t id) = 0;
    // v355: a node of the MIDI IN ring by its address (the hub's, the clock object's)
    virtual Vc2RingNode& midiInNode(uint32_t at) = 0;
};

struct Vc2SynthClock {
    static constexpr uint32_t kTask = 0x8994fdf8u;    // [FW] the clock task's event object
    static constexpr uint32_t kTakeEvent = 2;         // [FW] its event each ms
    static constexpr uint32_t kAlarmEvent = 1;        // [FW] its own alarm's, which it turns off
    static constexpr uint32_t kMask = 0x47;           // [FW] its wait's
    static constexpr uint32_t kSettingsEvent = 0x40;  // [FW] v355: its MIDI listener's (0x89313d3e)
    static constexpr uint32_t kAt = 0x8995cb40u;      // [FW] the clock object, a node of the MIDI IN ring
    Vc2ClockObject& object;
    Vc2TempoList& list;
    Vc2ScheduleTimer& schedule;
    // v355: the MIDI IN ring's head (0x899598a0), the listeners the input task tells every message it takes: the hub's
    // node, and the clock object while its clocks come from outside (its method there a bare return, 0x893133c0)
    uint32_t& midiIn;
    Vc2ClockParts& parts;

    static void build(Vc2ClockObject& o);             // row 20, 0x893133d0
    static void buildSchedule(Vc2ScheduleTimer& t);   // the first part row's (0x893903b0), its timer
    void ms();                                        // 0x89313000
    // The clock task's pass on what its wait took: 2 its take into the timers; 1 its own alarm off, the kernel's;
    // v355: 0x40 the settings its MIDI listener says moved, the clock's source and clocks out (0x89313d50), then the
    // patch's tempo (0x89313e10)
    void pass(uint32_t events);
    // v355: the clock task's MIDI listener (0x89313cb0, its object 0x8995cc08 on the hub) on a message's class and word:
    // the parameters' message of kind 4 or 6 (0x40) or of kind 0 (0x41) posts the task 0x40
    void heard(uint8_t cls, uint32_t word);
    void armSchedule();                               // the notes' task's start (0x8938fc50…0x8938fc62)

private:
    void clocksOut(uint32_t ticks);                   // 0x89313134…0x8931317c
    void sourceMoved();                               // v355: 0x89313d50
    void tempoMoved();                                // v355: 0x89313e10
    void fired(uint32_t timer);                       // a due timer's method by its class: the schedule's 0x8938fea0
    Vc2TempoTimers timers();
};
