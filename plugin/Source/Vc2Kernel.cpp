// rom.1's kernel as the VC-2's part runs on it (Vc2Kernel.h, v353). Each service masks the interrupts in the firmware,
// which here is nothing: the part runs on one thread. Each comment names the routine and the words it was read from.
#include "Vc2Kernel.h"

namespace {
constexpr uint32_t kHold = 0x10000u;         // [FW] a hold of the dispatch (0x88010250, 0x880102d0)
constexpr uint32_t kHolds = 0xffff0000u;     // [FW] the holds' half, which stops the round robin (0x8800fbd4)
constexpr uint32_t kLockOut = 0x80000000u;   // [FW] the lock-out's bit (0x88010290, 0x88010320)
constexpr uint16_t kAlarmEvent = 1;          // [FW] the event an alarm sets (0x8800fe18), which setInterval clears
} // namespace

Vc2Kernel::Vc2Kernel() { heads.fill(kNone); }

int& Vc2Kernel::link(int prev, int Vc2KernelTask::*next, int& head) {
    return prev == kNone ? head : tasks[prev].*next;
}

Vc2Kernel::Switch Vc2Kernel::start(const std::vector<std::pair<int, uint8_t>>& levels) {
    for (const auto& [level, quantum] : levels) {   // each TCB as rom.4's 0x89301680 sets it
        Vc2KernelTask t;
        t.level = level;
        t.quantum = t.count = quantum;
        tasks.push_back(t);
    }
    for (int t = (int)tasks.size() - 1; t >= 0; --t) makeReady(t);   // 0x893017be, the last first
    const int first = firstReady(0);                                // 0x88010ef0
    return first == current ? Switch{} : switchTo(first);
}

// 0x8800fa40: at its queue's head, ready, its round robin's count 1 when it has a quantum
void Vc2Kernel::makeReady(int t) {
    Vc2KernelTask& k = tasks[t];
    k.readyNext = heads[k.level];
    heads[k.level] = t;
    k.ready = 1;
    k.count = k.quantum != 0 ? 1 : 0;
}

// 0x8800fa70: out of its queue, waiting
void Vc2Kernel::unready(int t) {
    Vc2KernelTask& k = tasks[t];
    int prev = kNone;
    for (int p = heads[k.level]; p != kNone && p != t; p = tasks[p].readyNext) prev = p;
    link(prev, &Vc2KernelTask::readyNext, heads[k.level]) = k.readyNext;
    k.ready = 0;
    k.readyNext = kNone;
}

// 0x8800faa0: its queue's head to its queue's end, when another is behind it
void Vc2Kernel::rotate(int t) {
    Vc2KernelTask& k = tasks[t];
    if (k.readyNext == kNone) return;
    heads[k.level] = k.readyNext;
    int last = k.readyNext;
    while (tasks[last].readyNext != kNone) last = tasks[last].readyNext;
    tasks[last].readyNext = t;
    k.readyNext = kNone;
}

// 0x8800fad0 and the tail every switch shares (0x8801004e, 0x88010b4c, 0x8800fb32, 0x88010f1a): the task now running
// takes the events it waited for, which its wait answers
Vc2Kernel::Switch Vc2Kernel::switchTo(int t) {
    current = t;
    Vc2KernelTask& k = tasks[t];
    const auto answer = (uint16_t)(k.event & k.wait);
    k.event ^= answer;
    k.wait = 0;
    return { true, t, answer };
}

// The dispatchers' scan (0x88010036, 0x88010b22, 0x8800fb1a): the first queue's head from `level` on
int Vc2Kernel::firstReady(int level) const {
    for (int l = level; l < kLevels; ++l)
        if (heads[l] != kNone) return heads[l];
    return kNone;
}

// 0x8800ffc0: with none of the mask's events set the task stops waiting for them; with one set, it goes behind another
// of its level if there is one; otherwise it takes them at once. With the dispatch held or locked out, 0 at once.
Vc2Kernel::Switch Vc2Kernel::wait(uint16_t mask) {
    if (lock != 0) return { false, current, 0 };
    Vc2KernelTask& t = tasks[current];
    const auto now = (uint16_t)(t.event & mask);
    if (now != 0 && t.readyNext == kNone) {
        t.event ^= now;
        return { false, current, now };
    }
    if (now == 0) unready(current);
    else rotate(current);
    t.wait = mask;
    return switchTo(firstReady(t.level));
}

// 0x880100a0: the events set; a task they wake runs at once if it is at the running task's level or above. With the
// dispatch held or locked out, as from an interrupt (0x88010150).
Vc2Kernel::Switch Vc2Kernel::set(int task, uint16_t bits) {
    if (lock != 0) {
        raise(task, bits);
        return {};
    }
    Vc2KernelTask& t = tasks[task];
    t.event |= bits;
    if ((t.event & t.wait) == 0 || t.ready == 1) return {};
    makeReady(task);
    if (tasks[current].level >= t.level) return switchTo(task);
    return {};
}

// 0x88010190: the events set; a task waiting for one of them is woken, its dispatch put off
void Vc2Kernel::raise(int task, uint16_t bits) {
    Vc2KernelTask& t = tasks[task];
    t.event |= bits;
    if ((t.wait & bits) != 0) wake(task);
}

// 0x8800fd20: made ready, and the put-off dispatch for it, unless one is for a task at its level or above; the watchdog
// started for it (0x88011bf0)
void Vc2Kernel::wake(int t) {
    if (tasks[t].ready != 1) makeReady(t);
    if (freeze != kNone && tasks[freeze].level <= tasks[t].level) return;
    watchdog = true;
    freeze = t;
}

// 0x8800fb00: the put-off dispatch, when its task is at the running task's level or above
Vc2Kernel::Switch Vc2Kernel::putOffDispatch() {
    if (freeze == kNone) return {};
    const int f = freeze;
    freeze = kNone;
    if (tasks[f].level > tasks[current].level) return {};
    return switchTo(firstReady(tasks[f].level));
}

// 0x8800fb60: the watchdog stopped (0x88011c10), then the put-off dispatch unless the dispatch is held or locked out
Vc2Kernel::Switch Vc2Kernel::interruptEnd() {
    watchdog = false;
    return lock != 0 ? Switch{} : putOffDispatch();
}

// 0x8800fbb0: a level's head counts its quantum down; at its end it goes behind the next, and if it was running, the
// dispatch is put off for that next, unless one is for a task at the next's level or above
void Vc2Kernel::roundRobin(int level) {
    if ((lock & kHolds) != 0) return;
    const int h = heads[level];
    if (h == kNone || tasks[h].count == 0) return;
    Vc2KernelTask& t = tasks[h];
    if (--t.count != 0) return;
    t.count = t.quantum;
    const int next = t.readyNext;
    if (next == kNone) return;
    rotate(h);
    if (current != h || (freeze != kNone && tasks[freeze].level <= tasks[next].level)) return;
    freeze = next;
    watchdog = true;
}

// 0x8800fd50: on the alarm list at `ticks` on, before the first due then or later
void Vc2Kernel::alarmOn(int t, uint16_t ticks) {
    const uint32_t at = time + ticks;
    int prev = kNone, p = alarms;
    for (; p != kNone && tasks[p].alarmAt < at; p = tasks[p].alarmNext) prev = p;
    link(prev, &Vc2KernelTask::alarmNext, alarms) = t;
    tasks[t].alarmNext = p;
    tasks[t].alarmAt = at;
}

// 0x8800fdb0: off the alarm list, its own link and time left as they were
void Vc2Kernel::alarmOff(int t) {
    int prev = kNone;
    for (int p = alarms; p != kNone; prev = p, p = tasks[p].alarmNext)
        if (p == t) {
            link(prev, &Vc2KernelTask::alarmNext, alarms) = tasks[p].alarmNext;
            return;
        }
}

// 0x8800fdf0: a tick (its timer's acknowledgement is TMU0's, 0x88011be0): the round robin, then each alarm due now
// sets event 1 and is due again its interval on, then each cyclic handler that is on and due runs, due again its
// interval on
void Vc2Kernel::tick(const Handler& run) {
    ++time;
    roundRobin(roundLevel);
    for (int t = alarms; t != kNone && tasks[t].alarmAt == time; t = alarms) {
        raise(t, kAlarmEvent);
        alarms = tasks[t].alarmNext;
        alarmOn(t, tasks[t].interval);
    }
    for (Vc2KernelCyclic& c : cyclics) {
        if ((c.on & 1) == 0 || c.due != time) continue;
        c.due += c.interval;
        run(c.handler);
    }
}

// 0x880101e0: event 1 cleared, the interval kept, and the alarm due that many ticks on (none at 0)
void Vc2Kernel::setInterval(uint16_t ticks) {
    Vc2KernelTask& t = tasks[current];
    t.event &= (uint16_t)~kAlarmEvent;
    t.interval = ticks;
    alarmOff(current);
    if (ticks != 0) alarmOn(current, ticks);
}

// 0x88010810: the record from its spec, made by the running task, then on or off as the spec says (0x88010890)
void Vc2Kernel::cyclicMake(int k, int handler, uint16_t on, uint32_t interval) {
    Vc2KernelCyclic& c = cyclics[k];
    c.handler = handler;
    c.on = on;
    c.interval = interval;
    c.owner = current;
    cyclicSwitch(k, on);
}

void Vc2Kernel::cyclicClear(int k) { cyclics[k] = Vc2KernelCyclic {}; }

// 0x88010890: a made record on or off; on, it is due its interval on, off, 0
void Vc2Kernel::cyclicSwitch(int k, uint16_t on) {
    Vc2KernelCyclic& c = cyclics[k];
    if (c.handler == kNone) return;
    c.on = on;
    c.due = (on & 1) != 0 ? time + c.interval : 0;
}

void Vc2Kernel::holdDispatch() { lock += kHold; }

Vc2Kernel::Switch Vc2Kernel::releaseDispatch() {
    lock -= kHold;
    return lock == 0 ? putOffDispatch() : Switch{};
}

void Vc2Kernel::lockOut() { lock |= kLockOut; }

Vc2Kernel::Switch Vc2Kernel::unlockOut() {
    lock &= ~kLockOut;
    return lock == 0 ? putOffDispatch() : Switch{};
}
