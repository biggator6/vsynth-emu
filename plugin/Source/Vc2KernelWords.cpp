// rom.1's kernel's words as each side gives them (Vc2KernelWords.h, v355).
#include "Vc2KernelWords.h"
#include "Vc2KernelRig.h"

std::vector<Vc2KernelWord> vc2KernelTheirs(Sh4& c, int tasks, const Vc2KernelNames& n, bool watchdog) {
    using Rig = Vc2KernelRig;
    std::vector<Vc2KernelWord> w;
    for (int t = 0; t < tasks; ++t) {
        const uint32_t a = n.tcb(t);
        w.insert(w.end(), { { "event", t, c.rd16(a) }, { "wait", t, c.rd16(a + 2) }, { "interval", t, c.rd16(a + 6) },
                            { "ready", t, c.rd16(a + 0xa) }, { "quantum", t, c.rd8(a + 0xc) },
                            { "count", t, c.rd8(a + 0xd) }, { "level", t, (c.rd32(a + 0x14) - Rig::kReadyQueue) / 4 },
                            { "alarmNext", t, n.task(c.rd32(a + 0x1c)) }, { "alarmAt", t, c.rd32(a + 0x20) },
                            { "readyNext", t, n.task(c.rd32(a + 0x28)) } });
    }
    for (int l = 0; l < Vc2Kernel::kLevels; ++l)
        w.push_back({ "head", l, n.task(c.rd32(Rig::kReadyQueue + 4u * l)) });
    const uint32_t s = Rig::kSystem, freeze = c.rd32(s + 8);
    w.insert(w.end(), { { "current", 0, n.task(c.rd32(s)) }, { "time", 0, c.rd32(s + 4) },
                        { "freeze", 0, freeze == 0xffffffffu ? -1 : n.task(freeze) }, { "lock", 0, c.rd32(s + 0xc) },
                        { "alarms", 0, n.task(c.rd32(s + 0x10)) }, { "roundLevel", 0, c.rd32(s + 0x14) },
                        { "watchdog", 0, watchdog } });
    for (int k = 0; k < Vc2Kernel::kCyclics; ++k) {
        const uint32_t r = c.rd32(Rig::kCyclics + 4u * (uint32_t)k);
        w.insert(w.end(), { { "handler", k, n.handler(c.rd32(r)) }, { "on", k, c.rd16(r + 4) },
                            { "cyclicInterval", k, c.rd32(r + 8) }, { "due", k, c.rd32(r + 0xc) },
                            { "owner", k, n.task(c.rd32(r + 0x10)) } });
    }
    return w;
}

std::vector<Vc2KernelWord> vc2KernelOurs(const Vc2Kernel& k) {
    std::vector<Vc2KernelWord> w;
    for (int t = 0; t < (int)k.tasks.size(); ++t) {
        const Vc2KernelTask& a = k.tasks[(size_t)t];
        w.insert(w.end(), { { "event", t, a.event }, { "wait", t, a.wait }, { "interval", t, a.interval },
                            { "ready", t, a.ready }, { "quantum", t, a.quantum }, { "count", t, a.count },
                            { "level", t, a.level }, { "alarmNext", t, a.alarmNext }, { "alarmAt", t, a.alarmAt },
                            { "readyNext", t, a.readyNext } });
    }
    for (int l = 0; l < Vc2Kernel::kLevels; ++l) w.push_back({ "head", l, k.heads[(size_t)l] });
    w.insert(w.end(), { { "current", 0, k.current }, { "time", 0, k.time }, { "freeze", 0, k.freeze },
                        { "lock", 0, k.lock }, { "alarms", 0, k.alarms }, { "roundLevel", 0, k.roundLevel },
                        { "watchdog", 0, k.watchdog } });
    for (int c = 0; c < Vc2Kernel::kCyclics; ++c) {
        const Vc2KernelCyclic& r = k.cyclics[(size_t)c];
        w.insert(w.end(), { { "handler", c, r.handler }, { "on", c, r.on }, { "cyclicInterval", c, r.interval },
                            { "due", c, r.due }, { "owner", c, r.owner } });
    }
    return w;
}

std::string vc2KernelDifference(const std::vector<Vc2KernelWord>& a, const std::vector<Vc2KernelWord>& b) {
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        if (!(a[i] == b[i]))
            return std::string(a[i].what) + " " + std::to_string(a[i].whose) + ": rom.1's " +
                   std::to_string(a[i].value) + ", the port's " + std::to_string(b[i].value);
    }
    return "";
}
