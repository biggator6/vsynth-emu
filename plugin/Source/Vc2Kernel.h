#pragma once
// rom.1's kernel as the VC-2's part runs on it (v353): its tasks by level, their events and waits, the 1 ms tick's
// round robin, alarms and cyclic handlers, and the dispatch, ported from rom.1's event services (HARDWARE_FIRMWARE.md,
// "rom.1, the kernel" and "The kernel's event services"). It holds no task's code: it says which task runs next and
// what that task's wait answers, and the part runs the task's pass (xt_vc2kernel checks it against rom.1's own).
#include <array>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

// A task's control block, the words the event services use (a TCB; the names are the kernel's debug dump's)
struct Vc2KernelTask {
    uint16_t event = 0;      // +0 tcbEvent
    uint16_t wait = 0;       // +2 tcbWait: the events its wait takes
    uint16_t interval = 0;   // +6 tcbItvl: its own alarm's, in ticks
    uint16_t ready = 0;      // +0xa tcbStat: 1 in its ready queue, 0 waiting
    uint8_t quantum = 0;     // +0xc tcbQtmSize: its round robin's ticks
    uint8_t count = 0;       // +0xd tcbQtmCount: those left
    int level = 0;           // +0x14 tcbRqueue: its ready queue, readyQueue[level]
    int alarmNext = -1;      // +0x1c tcbAlink: the next on the alarm list, -1 for none
    uint32_t alarmAt = 0;    // +0x20 tcbAtime: the tick its alarm is due
    int readyNext = -1;      // +0x28 tcbRlink: the next in its ready queue, -1 for none
};

// A cyclic handler's record, one of the kernel's 16
struct Vc2KernelCyclic {
    int handler = -1;        // +0: the handler, by the number its maker gave; -1 for none
    uint16_t on = 0;         // +4: bit 0 on
    uint32_t interval = 0;   // +8: in ticks
    uint32_t due = 0;        // +0xc: the tick it runs at, 0 when off
    int owner = -1;          // +0x10: the task that made it
};

class Vc2Kernel {
public:
    static constexpr int kLevels = 32, kCyclics = 16, kNone = -1;
    static constexpr int kRoundLevel = 16;   // [FW] roundLevel as the kernel's start leaves it (0x88010c64)
    // A due cyclic handler's run, by its number, from the tick (rom.1's 0x8800fdf0 calls each due record's +0)
    using Handler = std::function<void(int)>;
    // What a service did to the running task: whether it switched, to which task, and what that task's wait answers
    struct Switch {
        bool switched = false;
        int to = kNone;
        uint16_t answer = 0;
    };

    std::vector<Vc2KernelTask> tasks;
    std::array<int, kLevels> heads;          // readyQueue[]: each level's first ready task
    std::array<Vc2KernelCyclic, kCyclics> cyclics {};
    int current = kNone;                     // curTask
    uint32_t time = 0;                       // sysTime, in ticks
    int freeze = kNone;                      // freezTask: the task the put-off dispatch is for
    uint32_t lock = 0;                       // lockFlag: +0x10000 a hold of the dispatch, bit 31 the lock-out
    int alarms = kNone;                      // alarmTop: the alarm list, soonest first
    int roundLevel = kRoundLevel;            // roundLevel: the level the tick's round robin turns
    bool watchdog = false;                   // the watchdog's interval interrupt started, for the put-off dispatch

    Vc2Kernel();
    // The tasks as an application's start makes them (rom.4's 0x89301790): one a level and quantum, in order, made
    // ready the last first, so each queue's head is its first; then the first dispatch (0x88010ef0)
    Switch start(const std::vector<std::pair<int, uint8_t>>& levels);
    Switch wait(uint16_t mask);                // 0x8800ffc0, the running task's
    Switch set(int task, uint16_t bits);       // 0x880100a0, from a task
    void raise(int task, uint16_t bits);       // 0x88010190, from an interrupt
    Switch interruptEnd();                     // 0x8800fb60, the watchdog's interrupt, once started
    void tick(const Handler& run);             // 0x8800fdf0, TMU0's
    void setInterval(uint16_t ticks);          // 0x880101e0, the running task's own alarm
    void cyclicMake(int k, int handler, uint16_t on, uint32_t interval);   // 0x88010810
    void cyclicClear(int k);                   // 0x88010810 given -1 for its spec
    void cyclicSwitch(int k, uint16_t on);     // 0x88010890
    void holdDispatch();                       // 0x88010250
    Switch releaseDispatch();                  // 0x880102d0
    void lockOut();                            // 0x88010290
    Switch unlockOut();                        // 0x88010320

private:
    void makeReady(int t);                     // 0x8800fa40
    void unready(int t);                       // 0x8800fa70
    void rotate(int t);                        // 0x8800faa0
    Switch switchTo(int t);                    // 0x8800fad0, the switch and every switch's tail
    int firstReady(int level) const;           // the dispatchers' scan, from a level up
    Switch putOffDispatch();                   // 0x8800fb00
    void roundRobin(int level);                // 0x8800fbb0
    void wake(int t);                          // 0x8800fd20
    void alarmOn(int t, uint16_t ticks);       // 0x8800fd50
    void alarmOff(int t);                      // 0x8800fdb0
    int& link(int prev, int Vc2KernelTask::*next, int& head);   // `head`, or prev's next, as a list's link
};
