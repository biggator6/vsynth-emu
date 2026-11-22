// xt_vc2voices' update queue's draws and compare (v279's, their own file from v312): the queue's 36 bytes, and from
// v312 the static M's node when the port's queue is given it, written into rom.4's memory before an event and compared
// after it; the queue's links drawn as one list.  The fourth, sixth and seventh parts'.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2VoiceClassCheck.h"
#include <algorithm>

namespace {
using V = Vc2PartVoices;
using Q = Vc2UpdateQueue;
} // namespace

void ClassSides::giveNode() {
    hasNode = true;
    node.vtable = kClassStaticNodeClass;
    queue.setNode(kVc2StaticNodeAt, node);
}

uint32_t ClassSides::classOf(uint32_t at) {
    return hasNode && at == kVc2StaticNodeAt ? node.vtable : r.a.s.port.at(at).vtable;
}

void classQueueWrite(ClassSides& c) {
    c.r.a.s.fw.cpu.write(Q::kAt, &c.queue.q, sizeof c.queue.q);
    if (c.hasNode) c.r.a.s.fw.cpu.write(kVc2StaticNodeAt, &c.node, sizeof c.node);
}

std::string classQueueDiffer(ClassSides& c) {
    Vc2QueueWords theirs;
    c.r.a.s.fw.cpu.read(Q::kAt, &theirs, sizeof theirs);
    const auto* ours = reinterpret_cast<const uint32_t*>(&c.queue.q);
    const auto* rom = reinterpret_cast<const uint32_t*>(&theirs);
    for (uint32_t k = 0; k < sizeof theirs / 4; ++k) {
        if (ours[k] == rom[k]) continue;
        char t[96];
        std::snprintf(t, sizeof t, "the queue's +0x%02x: %08x, against %08x", 4 * k, ours[k], rom[k]);
        return t;
    }
    if (!c.hasNode) return "";
    return vc2RegionsDifference(c.r.a.s.fw, { { kVc2StaticNodeAt, &c.node, sizeof c.node, "the static M's node" } });
}

// The voices' queue links one list (and the node given's, when it is): a few on the queue in a drawn order, the rest
// linked to themselves, and one time in four a node's link aimed elsewhere; every node on queue 0; the queue's current
// node one of them, the head or none
void drawQueue(ClassSides& c, std::mt19937& rng) {
    V& port = c.r.a.s.port;
    std::array<uint32_t, V::kVoices + 1> nodes {};
    for (int i = 0; i < V::kVoices; ++i) nodes[(size_t)i] = port.addressOf(i);
    nodes[V::kVoices] = kVc2StaticNodeAt;
    const std::span<uint32_t> all(nodes.data(), V::kVoices + (c.hasNode ? 1 : 0));
    std::ranges::shuffle(all, rng);
    const size_t queued = rng() % 8;
    uint32_t last = Q::kHeadAt;
    for (size_t i = 0; i < all.size(); ++i) {
        const Vc2QueueSlots n = c.queue.slotsOf(all[i], port);
        n.queue = 0;
        n.queued = rng() % 2;
        n.next = all[i];
        n.prev = all[i];
        if (i >= queued) continue;
        n.prev = last;
        c.queue.nextOf(last, port) = all[i];
        last = all[i];
    }
    c.queue.nextOf(last, port) = Q::kHeadAt;
    c.queue.q.prev = last;
    const uint32_t odd = all[rng() % all.size()], to = rng() % 3 ? all[rng() % all.size()] : Q::kHeadAt;
    if (rng() % 4 == 0) {
        const Vc2QueueSlots o = c.queue.slotsOf(odd, port);
        (rng() % 2 ? o.next : o.prev) = to;
    }
    const uint32_t currents[4] = { 0, Q::kHeadAt, all[0], all[rng() % all.size()] };
    c.queue.q.current = currents[rng() % 4];
    c.queue.q.vtable = rng() % 4 ? Q::kHeadVtable : (uint32_t)rng();
    c.queue.q.post = (uint32_t)rng();
    c.queue.q.lock = (uint32_t)rng();
}
#endif
