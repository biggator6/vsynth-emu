// xt_vc2voices' sixth part (v310): the update task's pass, vc2UpdateTaskPass, against rom.4's own loop body on its own
// voices and update queue, run from the loop's top (0x89395310) until it comes back there (v309's way), with the
// registers the task's prologue leaves; after the listeners' part on the seed after its.  The voice class's part's sides,
// hooks and draws, and beyond them: the wait (0x893017f0) answering the events drawn, both voice classes' +0x0c (the
// node's update) logging the node and the time struct's address and answering from the voices' draws, and the pitch
// source 0x89394a60 and the voices' update 0x893b16a0 logged.  Before each pass the events (each of the two bits half
// the time; now and then none or any word), the time struct's count 0, small or any and its last any, the ticks a few
// past the last, past a half's wrap, or any; the queue's links drawn again until its walk ends at the head and every
// node on it is linked both ways.  After each the voices' and keys' spans, the queue's words, the time struct and the
// ticks, the calls.  Not drawn: a node on the queue but a voice (v279's queue takes no other), a queue whose nodes or
// head are not each linked both ways (rom.4's walk need not end on one) but for one node now and then whose +0 link is
// itself.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2UpdateTask.h"
#include "Vc2VoiceClassCheck.h"

namespace {
using V = Vc2PartVoices;
using Q = Vc2UpdateQueue;
constexpr uint32_t kUpdateLoopAt = 0x89395310, kUpdateWaitAt = 0x893017f0;                       // [FW]
constexpr uint32_t kTicksAt = 0x8993d080, kTimeAt = 0x8993d084, kPitchAt = 0x89989748;          // [FW]
constexpr uint32_t kPitchSourceAt = 0x89394a60, kVoicesUpdateAt = 0x893b16a0, kNodeUpdateEntry = 0x0c;   // [FW]
// [FW] the registers the task's prologue leaves: r8 the OS's wait, r9 the interrupts' mask; r12 the queue
constexpr uint32_t kOsWaitAt = 0x893018b0, kInterruptMask = 0xffffff0f;

// The voice class's sides, with the events, time struct and ticks drawn
struct UpdateSides : ClassSides {
    using ClassSides::ClassSides;
    uint32_t events = 0, ticks = 0;
    Vc2TaskTime time {};
};

uint32_t updateAnswer(VoicesSides& s) { return s.draws.next(s.draws.rom).updated; }

void updateInstallRom(UpdateSides& u) {
    classInstallRom(u);
    VoicesSides& s = u.r.a.s;
    assignHookSlot(s, kVoicesTask, kNodeUpdateEntry, 1, updateAnswer);
    s.fw.cpu.hook(kUpdateWaitAt, [&u](Sh4& c) { c.ret(u.events); });
    s.fw.cpu.hook(kPitchSourceAt, [&s](Sh4& c) {
        s.rom.push_back({ kVoicesTask, kPitchSourceAt, c.r[4] });
        c.ret(0);
    });
    s.fw.cpu.hook(kVoicesUpdateAt, [&s](Sh4& c) {
        s.rom.push_back({ kVoicesTask, kVoicesUpdateAt, 0 });
        c.ret(0);
    });
}

// The port's parts: the OS's calls as the voice class's part logs them, the rest as their hooks log them
class UpdatePortParts final : public Vc2UpdateTaskParts {
public:
    explicit UpdatePortParts(UpdateSides& u) : u_(u), s_(u.r.a.s) {}
    void wait(uint32_t object) override { u_.parts.wait(object); }
    void signal(uint32_t object) override { u_.parts.signal(object); }
    void post(uint32_t object) override { u_.parts.post(object); }
    uint32_t nodeUpdate(uint32_t node, uint32_t time) override {
        const VoicesCall slot = voicesSlotCall(s_.fw, kVoicesTask, node, s_.port.at(node).vtable, kNodeUpdateEntry);
        s_.mine.push_back({ kVoicesTask, slot.routine, slot.object, { time } });
        return s_.draws.next(s_.draws.port).updated;
    }
    void pitchSource() override { s_.mine.push_back({ kVoicesTask, kPitchSourceAt, kPitchAt }); }
    void voicesUpdate() override { s_.mine.push_back({ kVoicesTask, kVoicesUpdateAt, 0 }); }

private:
    UpdateSides& u_;
    VoicesSides& s_;
};

// Whether the walk from the head by the +4 links comes back to it within the voices' count, each node and the head
// linked both ways (on a list broken otherwise rom.4's walk need not end)
bool walkSound(UpdateSides& u) {
    V& port = u.r.a.s.port;
    uint32_t at = Q::kHeadAt;
    for (int k = 0; k <= V::kVoices; ++k) {
        const uint32_t before = u.queue.prevOf(at, port);
        if (u.queue.nextOf(before, port) != at) return false;
        at = before;
        if (at == Q::kHeadAt) return true;
    }
    return false;
}

// One time in eight a node on the walk with its +0 link to itself, which the walk passes by (the walk still ends: it
// goes by the +4 links)
void unlinkOneOnWalk(UpdateSides& u, std::mt19937& rng) {
    V& port = u.r.a.s.port;
    std::vector<uint32_t> walked;
    for (uint32_t at = u.queue.prevOf(Q::kHeadAt, port); at != Q::kHeadAt; at = u.queue.prevOf(at, port))
        walked.push_back(at);
    if (walked.empty() || rng() % 8 != 0) return;
    const uint32_t node = walked[rng() % walked.size()];
    port.at(node).queueNext = node;
}

// The voice class's draws, the queue's again until its walk is sound, the events, the time struct and the ticks
void updateDraw(UpdateSides& u, std::mt19937& rng, bool first) {
    classDraw(u, rng, first);
    while (!walkSound(u)) drawQueue(u, rng);
    unlinkOneOnWalk(u, rng);
    const uint32_t events[4] = { 1 + rng() % 3, 1 + rng() % 3, 0, (uint32_t)rng() };
    u.events = events[rng() % 4];
    const int16_t counts[3] = { 0, (int16_t)(1 + rng() % 4), (int16_t)rng() };
    u.time.elapsed = counts[rng() % 3];
    u.time.last = (uint32_t)rng();
    const uint32_t ticks[3] = { u.time.last + rng() % 8, u.time.last + 0x8000 + rng() % 0x10000, (uint32_t)rng() };
    u.ticks = ticks[rng() % 3];
}

std::string taskPassEvent(UpdateSides& u, std::mt19937&) {
    VoicesSides& s = u.r.a.s;
    s.fw.cpu.write(kTimeAt, &u.time, sizeof u.time);
    s.fw.cpu.wr32(kTicksAt, u.ticks);
    s.fw.cpu.r[8] = kOsWaitAt;
    s.fw.cpu.r[9] = kInterruptMask;
    s.fw.cpu.r[12] = Q::kAt;
    // one-off: the pass alone, entered at the loop's top and left at its return there (v309's way)
    uint32_t back = 0;
    bool entered = false;
    s.fw.cpu.hook(kUpdateLoopAt, [&back, &entered](Sh4& m) {
        if (entered) m.pc = back;
        back = entered ? back : m.pr;
        entered = true;
    });
    ++u.n.byEvent[0];
    const std::string what = classOnBoth(u, { kUpdateLoopAt, "the update task's pass", nullptr }, {}, [&] {
        UpdatePortParts parts { u };
        Vc2UpdateTaskWorld w { u.queue, s.port, u.time, u.ticks, parts };
        vc2UpdateTaskPass(w, u.events);
        return 0u;
    });
    s.fw.cpu.unhook(kUpdateLoopAt);
    if (!what.empty()) return what;
    return vc2RegionsDifference(s.fw, { { kTimeAt, &u.time, sizeof u.time, "the time struct" },
                                        { kTicksAt, &u.ticks, sizeof u.ticks, "the ticks" } });
}

using UpdateEvent = std::string (*)(UpdateSides&, std::mt19937&);
constexpr UpdateEvent kUpdateEvents[] = { taskPassEvent };
std::span<const UpdateEvent> updateEvents() { return kUpdateEvents; }

void updateCount(UpdateSides& u) {
    ++u.n.events;
    u.n.byEvent[1] += vc2CountCalls(u.r.a.s.mine, kVoicesTask);
    u.n.osCalls += vc2CountCalls(u.r.a.s.mine, kVoicesOs);
}

void sayUpdateCounts(const ClassCounts& n) {
    std::printf("update task: %ld events: %ld passes, %ld task calls, %ld OS calls\n", n.events, n.byEvent[0],
                n.byEvent[1], n.osCalls);
}
} // namespace

long vc2UpdateTaskPart(VoicesSides& s, const Vc2Run& run) {
    return voicesPart<UpdateSides, updateInstallRom, classStart, updateDraw, updateEvents, updateCount,
                      sayUpdateCounts>(s, run, 5);
}
#endif
