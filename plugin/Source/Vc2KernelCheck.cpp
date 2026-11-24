// xt_vc2kernel <rom.4> [cases] [seed] (v353): Vc2Kernel, the port of rom.1's event services, against rom.1's own in the
// interpreter (rom.1 beside rom.4, Vc2KernelRig). Each case starts both on drawn tasks (levels, the round robin's 16
// among them, quanta, and an idle task at 31 last, which never waits), then runs drawn services on both: waits, event
// sets from the running task and from interrupts with the watchdog's interrupt after, ticks with cyclic handlers that
// set events, a task's own alarm, the records made and switched, the dispatch's holds and lock-out. After each it
// compares every task's TCB words, readyQueue, the system's words, the 16 cyclic records, the watchdog, and whether the
// service switched, to which task and what that task's wait took. XT_VC2KERNEL_FIRST=1 says where a case first
// differs; XT_VC2KERNEL_STOP=1 stops there. XT_VC2KERNEL_MAIN only.
#ifdef XT_VC2KERNEL_MAIN
#include "Vc2Kernel.h"
#include "Vc2KernelRig.h"
#include "XtArgs.h"
#include "XtSwitches.h"
#include <array>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {
using Rig = Vc2KernelRig;
using Switch = Vc2Kernel::Switch;
constexpr int kSteps = 200, kHandlers = 4, kCyclicsDrawn = 4, kMostHolds = 3;
constexpr int kIdleLevel = 31;
constexpr int kLevels[] = { 0, 3, 14, 16, 16, 16, 24, 27 };   // rom.4's own, the round robin's three times as often
constexpr uint8_t kQuanta[] = { 0, 1, 2, 8 };
constexpr uint16_t kEvents[] = { 1, 2, 4, 3, 5, 6, 0x10, 0x100, 0x3fe, 0x1000 };
constexpr uint16_t kIntervals[] = { 0, 1, 2, 3, 5, 10 };
constexpr uint32_t kLockOutBit = 0x80000000u;
constexpr uint32_t kNoSpec = 0xffffffffu;   // [FW] the maker's spec that clears its record (0x88010830)

// One word of the state, by what it is and whose, as either side gives it
struct Word {
    const char* what;
    int whose;
    int64_t value;
    bool operator==(const Word&) const = default;
};

int64_t freezeOf(uint32_t a) { return a == 0xffffffffu ? -1 : Rig::task(a); }
int64_t handlerOf(uint32_t a) { return a == 0 ? -1 : (int64_t)((a - Rig::kHandlers) / 0x10u); }

std::vector<Word> theirs(Rig& rig, int tasks) {
    Sh4& c = rig.cpu;
    std::vector<Word> w;
    for (int t = 0; t < tasks; ++t) {
        const uint32_t a = Rig::tcb(t);
        w.insert(w.end(), { { "event", t, c.rd16(a) }, { "wait", t, c.rd16(a + 2) }, { "interval", t, c.rd16(a + 6) },
                            { "ready", t, c.rd16(a + 0xa) }, { "quantum", t, c.rd8(a + 0xc) },
                            { "count", t, c.rd8(a + 0xd) }, { "level", t, (c.rd32(a + 0x14) - Rig::kReadyQueue) / 4 },
                            { "alarmNext", t, Rig::task(c.rd32(a + 0x1c)) }, { "alarmAt", t, c.rd32(a + 0x20) },
                            { "readyNext", t, Rig::task(c.rd32(a + 0x28)) } });
    }
    for (int l = 0; l < Vc2Kernel::kLevels; ++l)
        w.push_back({ "head", l, Rig::task(c.rd32(Rig::kReadyQueue + 4u * l)) });
    const uint32_t s = Rig::kSystem;
    w.insert(w.end(), { { "current", 0, Rig::task(c.rd32(s)) }, { "time", 0, c.rd32(s + 4) },
                        { "freeze", 0, freezeOf(c.rd32(s + 8)) }, { "lock", 0, c.rd32(s + 0xc) },
                        { "alarms", 0, Rig::task(c.rd32(s + 0x10)) }, { "roundLevel", 0, c.rd32(s + 0x14) },
                        { "watchdog", 0, rig.watchdog } });
    for (int k = 0; k < Vc2Kernel::kCyclics; ++k) {
        const uint32_t r = rig.cyclic(k);
        w.insert(w.end(), { { "handler", k, handlerOf(c.rd32(r)) }, { "on", k, c.rd16(r + 4) },
                            { "cyclicInterval", k, c.rd32(r + 8) }, { "due", k, c.rd32(r + 0xc) },
                            { "owner", k, Rig::task(c.rd32(r + 0x10)) } });
    }
    return w;
}

std::vector<Word> ours(const Vc2Kernel& k) {
    std::vector<Word> w;
    for (int t = 0; t < (int)k.tasks.size(); ++t) {
        const Vc2KernelTask& a = k.tasks[t];
        w.insert(w.end(), { { "event", t, a.event }, { "wait", t, a.wait }, { "interval", t, a.interval },
                            { "ready", t, a.ready }, { "quantum", t, a.quantum }, { "count", t, a.count },
                            { "level", t, a.level }, { "alarmNext", t, a.alarmNext }, { "alarmAt", t, a.alarmAt },
                            { "readyNext", t, a.readyNext } });
    }
    for (int l = 0; l < Vc2Kernel::kLevels; ++l) w.push_back({ "head", l, k.heads[l] });
    w.insert(w.end(), { { "current", 0, k.current }, { "time", 0, k.time }, { "freeze", 0, k.freeze },
                        { "lock", 0, k.lock }, { "alarms", 0, k.alarms }, { "roundLevel", 0, k.roundLevel },
                        { "watchdog", 0, k.watchdog } });
    for (int c = 0; c < Vc2Kernel::kCyclics; ++c) {
        const Vc2KernelCyclic& r = k.cyclics[c];
        w.insert(w.end(), { { "handler", c, r.handler }, { "on", c, r.on }, { "cyclicInterval", c, r.interval },
                            { "due", c, r.due }, { "owner", c, r.owner } });
    }
    return w;
}

std::string difference(Rig& rig, const Vc2Kernel& k) {
    const std::vector<Word> a = theirs(rig, (int)k.tasks.size()), b = ours(k);
    for (size_t i = 0; i < a.size(); ++i)
        if (!(a[i] == b[i]))
            return std::string(a[i].what) + " " + std::to_string(a[i].whose) + ": rom.1's " +
                   std::to_string(a[i].value) + ", the port's " + std::to_string(b[i].value);
    return "";
}

// A case: both sides, the draws, the cyclic handlers' tasks and events, and the counts
struct Case {
    Rig& rig;
    Vc2Kernel& port;
    std::mt19937& rng;
    std::array<std::pair<int, uint16_t>, kHandlers> handlers {};
    int tasks = 0, holds = 0;
    long switches = 0, ran = 0;
    template <class T, size_t N> T pick(const T (&from)[N]) { return from[rng() % N]; }
    uint16_t events() { return rng() % 4 == 0 ? (uint16_t)rng() : pick(kEvents); }
    int anyTask() { return (int)(rng() % (uint32_t)tasks); }
    bool idle() const { return port.current == tasks - 1; }
};

// A port's service that switches nothing, as one that gives a Switch
template <class F>
auto quiet(F f) {
    return [f] {
        f();
        return Switch {};
    };
}

// What a service's switch was on each side, against the other's: the task it switched to and what that task took;
// with none, the running task and, for a wait (`answers`), what it took at once, which rom.1's gives back
std::string switchDifference(Case& c, const Switch& s, uint32_t r0, bool answers) {
    const bool switched = !c.rig.switches.empty();
    c.switches += switched;
    const int to = switched ? Rig::task(c.rig.switches.back().second) : c.port.current;
    const uint32_t answer = switched ? c.rig.answer : (answers ? r0 : 0);
    const int portTo = s.switched ? s.to : c.port.current;
    const uint32_t portAnswer = s.switched || answers ? s.answer : 0;
    if (switched == s.switched && to == portTo && answer == portAnswer) return "";
    char t[128];
    std::snprintf(t, sizeof t, "the switch: rom.1's %d to %d taking %04x, the port's %d to %d taking %04x", switched,
                  to, answer, s.switched, portTo, portAnswer);
    return t;
}

// rom.1's `routine` on `args` and the port's `mine`, then the switches compared
template <class Mine>
std::string both(Case& c, uint32_t routine, std::initializer_list<uint32_t> args, Mine mine, bool answers = false) {
    c.rig.switches.clear();
    uint32_t r0 = 0;
    if (!c.rig.call(routine, args, &r0)) return "rom.1's service faults: " + c.rig.cpu.error();
    return switchDifference(c, mine(), r0, answers);
}

// A tick on both, its cyclic handlers each setting their task's events
std::string bareTick(Case& c) {
    const auto handler = [&c](int h) { c.port.raise(c.handlers[h].first, c.handlers[h].second); };
    return both(c, Rig::kTick, {}, quiet([&c, handler] { c.port.tick(handler); }));
}

// The watchdog's interrupt on both, when the services before started it; one time in four a tick first, as TMU0's
// interrupt may be taken before the watchdog's (the round robin then meets a put-off dispatch)
std::string interruptEnd(Case& c) {
    if (!c.rig.watchdog) return "";
    std::string what = c.rng() % 4 == 0 ? bareTick(c) : "";
    return what.empty() ? both(c, Rig::kWatchdogIrq, {}, [&c] { return c.port.interruptEnd(); }) : what;
}

// A wait, its mask meeting the running task's pending events half the time (the wait's way behind a sibling)
std::string waitStep(Case& c) {
    if (c.idle()) return "";
    const uint16_t pending = c.port.tasks[c.port.current].event;
    const auto mask = (uint16_t)(c.events() | (c.rng() % 2 == 0 ? pending : 0));
    return both(c, Rig::kWait, { mask }, [&c, mask] { return c.port.wait(mask); }, true);
}

// An event set on a drawn task, the running one a time in four, as a task's own
std::string setStep(Case& c) {
    const int t = c.rng() % 4 == 0 ? c.port.current : c.anyTask();
    const uint16_t bits = c.events();
    return both(c, Rig::kSet, { Rig::tcb(t), bits }, [&c, t, bits] { return c.port.set(t, bits); });
}

std::string raiseStep(Case& c) {
    const int t = c.anyTask();
    const uint16_t bits = c.events();
    std::string what = both(c, Rig::kRaise, { Rig::tcb(t), bits }, quiet([&c, t, bits] { c.port.raise(t, bits); }));
    return what.empty() ? interruptEnd(c) : what;
}

std::string tickStep(Case& c) {
    std::string what = bareTick(c);
    return what.empty() ? interruptEnd(c) : what;
}

std::string intervalStep(Case& c) {
    if (c.idle()) return "";
    const uint16_t n = c.pick(kIntervals);
    return both(c, Rig::kInterval, { n }, quiet([&c, n] { c.port.setInterval(n); }));
}

// A record cleared, switched or made, one in four, four and two
std::string cyclicStep(Case& c) {
    const int k = (int)(c.rng() % kCyclicsDrawn), h = (int)(c.rng() % kHandlers), way = (int)(c.rng() % 4);
    const auto on = (uint16_t)(c.rng() % 4);
    const uint32_t interval = 1 + c.rng() % 5;
    const uint32_t spec[3] = { Rig::handler(h), on, interval };
    c.rig.cpu.write(Rig::kSpec, spec, sizeof spec);
    const uint32_t record = c.rig.cyclic(k);
    if (way == 0)
        return both(c, Rig::kCyclicMake, { record, kNoSpec }, quiet([&c, k] { c.port.cyclicClear(k); }));
    if (way == 1)
        return both(c, Rig::kCyclicSwitch, { record, on }, quiet([&c, k, on] { c.port.cyclicSwitch(k, on); }));
    return both(c, Rig::kCyclicMake, { record, Rig::kSpec },
                quiet([&c, k, h, on, interval] { c.port.cyclicMake(k, h, on, interval); }));
}

std::string holdStep(Case& c) {
    if (c.holds == kMostHolds) return "";
    ++c.holds;
    return both(c, Rig::kHold, {}, quiet([&c] { c.port.holdDispatch(); }));
}

std::string releaseStep(Case& c) {
    if (c.holds == 0) return "";
    --c.holds;
    return both(c, Rig::kRelease, {}, [&c] { return c.port.releaseDispatch(); });
}

// The lock-out undone when it is on; otherwise put on one time in three, so that most services run without it
std::string lockOutStep(Case& c) {
    if ((c.port.lock & kLockOutBit) != 0)
        return both(c, Rig::kUnlockOut, {}, [&c] { return c.port.unlockOut(); });
    if (c.rng() % 3 != 0) return "";
    return both(c, Rig::kLockOut, {}, quiet([&c] { c.port.lockOut(); }));
}

using Step = std::string (*)(Case&);
// The services by how often each is drawn: a hold's release twice as often as a hold, so the dispatch runs mostly free
constexpr Step kStepsDrawn[] = { waitStep, waitStep, waitStep, setStep, setStep, setStep, raiseStep, raiseStep,
                                 tickStep, tickStep, tickStep, intervalStep, cyclicStep, holdStep, releaseStep,
                                 releaseStep, lockOutStep };

// A case on drawn tasks, its services until one differs; what differs, or nothing
std::string kernelCase(Rig& rig, std::mt19937& rng, long& ran, long& switches) {
    std::vector<std::pair<int, uint8_t>> tasks;
    const int n = 2 + (int)(rng() % 5);
    for (int t = 0; t < n; ++t) tasks.emplace_back(kLevels[rng() % std::size(kLevels)], kQuanta[rng() % 4]);
    tasks.emplace_back(kIdleLevel, 0);
    Vc2Kernel port;
    Case c { rig, port, rng };
    c.tasks = (int)tasks.size();
    for (auto& [t, e] : c.handlers) {
        t = c.anyTask();
        e = c.events();
    }
    for (int h = 0; h < kHandlers; ++h) {
        const auto [t, e] = c.handlers[h];
        rig.cpu.hook(Rig::handler(h), [t, e](Sh4& cpu) {   // a handler sets its task's events, as rom.4's do
            cpu.r[4] = Rig::tcb(t);
            cpu.r[5] = e;
            cpu.pc = Rig::kRaise;
        });
    }
    if (!rig.begin(tasks)) return "rom.1's start faults: " + rig.cpu.error();
    std::string what = switchDifference(c, port.start(tasks), 0, false);
    if (what.empty()) what = difference(rig, port);
    for (int s = 0; s < kSteps && what.empty(); ++s, ++c.ran) {
        what = kStepsDrawn[rng() % std::size(kStepsDrawn)](c);
        if (what.empty()) what = difference(rig, port);
        if (!what.empty()) what = "service " + std::to_string(s) + ", " + what;
    }
    ran += c.ran;
    switches += c.switches;
    return what;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "xt_vc2kernel <rom.4> [cases] [seed]\n");
        return 2;
    }
    const int cases = argc > 2 ? intArg(argv[2]).value_or(-1) : 40;
    const int seed = argc > 3 ? intArg(argv[3]).value_or(-1) : 2024;
    Rig rig;
    std::string why;
    if (cases < 0 || seed < 0 || !rig.load(argv[1], why)) {
        std::fprintf(stderr, "xt_vc2kernel: %s\n", why.empty() ? "an argument that isn't a number" : why.c_str());
        return 2;
    }
    std::mt19937 rng((uint32_t)seed);
    const bool first = xtSwitchIs("XT_VC2KERNEL_FIRST", "1"), stop = xtSwitchIs("XT_VC2KERNEL_STOP", "1");
    long ran = 0, switches = 0, different = 0;
    for (int k = 0; k < cases; ++k) {
        const std::string what = kernelCase(rig, rng, ran, switches);
        if (what.empty()) continue;
        ++different;
        if (first) std::printf("  case %d: %s\n", k, what.c_str());
        if (stop) break;
    }
    std::printf("kernel: %d cases, %ld services, %ld switches\n", cases, ran, switches);
    std::printf("xt_vc2kernel: %ld different\n", different);
    return different == 0 ? 0 : 1;
}
#endif
