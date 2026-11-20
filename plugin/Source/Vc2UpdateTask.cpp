// The VC-2 part's update task, its pass: see Vc2UpdateTask.h (v310).  Its reads, calls and writes in rom.4's order.
#include "Vc2UpdateTask.h"

namespace {
using Q = Vc2UpdateQueue;
using W = Vc2UpdateTaskWorld;
constexpr uint32_t kTimer = 2, kUpdate = 1;      // [FW] the events
constexpr uint32_t kTimeAt = 0x8993d084;         // [FW] the time struct
constexpr int32_t kTickWrap = 0x10000;           // [FW] a tick count's half wraps
constexpr uint32_t kAnswerBits = 0xffff;         // [FW] the answer's half a node keeps

// The ticks since the last pass, as a half, into the time struct
void measure(W& w) {
    int32_t d = static_cast<int16_t>(static_cast<uint16_t>(w.ticks - w.time.last));
    if (d < 0) d += kTickWrap;
    w.time.elapsed = static_cast<int16_t>(d);
    w.time.last += static_cast<uint32_t>(d);
}

// Each node from the head's +4 link: its update under the queue's lock, taken off on an answer of 0
void runNodes(W& w) {
    Q& q = w.queue;
    uint32_t at = q.prevOf(Q::kHeadAt, w.voices);
    while (at != Q::kHeadAt) {
        q.q.current = at;
        w.parts.wait(q.q.lock);
        Vc2VoiceNode& n = w.voices.at(at);
        if (n.queueNext != at) n.queued = w.parts.nodeUpdate(at, kTimeAt) & kAnswerBits;
        w.parts.signal(q.q.lock);
        if (n.queued == 0 && n.queueNext != at) {
            q.q.current = n.queueNext;
            q.unlink(at, w.voices);
        }
        at = q.prevOf(q.q.current, w.voices);
    }
    q.q.current = at;
}
} // namespace

void vc2UpdateTaskPass(Vc2UpdateTaskWorld& w, uint32_t events) {
    if ((events & kTimer) != 0) {
        if (w.time.elapsed != 0) runNodes(w);
        measure(w);
    }
    if ((events & kUpdate) == 0) return;
    w.parts.pitchSource();
    w.parts.voicesUpdate();
}
