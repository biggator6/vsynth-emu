// The VC-2 part's update queue: see Vc2VoiceQueue.h (v279).  Each routine's reads, calls and writes in rom.4's order.
#include "Vc2VoiceQueue.h"

namespace {
constexpr uint32_t kQueueBytes = sizeof(Vc2QueueWords);
} // namespace

uint32_t& Vc2UpdateQueue::nextOf(uint32_t node, Vc2PartVoices& v) {
    return node == kHeadAt ? q.next : v.at(node).queueNext;
}

uint32_t& Vc2UpdateQueue::prevOf(uint32_t node, Vc2PartVoices& v) {
    return node == kHeadAt ? q.prev : v.at(node).queuePrev;
}

void Vc2UpdateQueue::construct() {
    q.next = kHeadAt;
    q.prev = kHeadAt;
    q.vtable = kHeadVtable;
    q.current = 0;
}

void Vc2UpdateQueue::staticInit() { construct(); }

void Vc2UpdateQueue::link(uint32_t node, uint32_t after, Vc2PartVoices& v) {
    const uint32_t next = nextOf(after, v);
    nextOf(node, v) = next;
    prevOf(node, v) = after;
    nextOf(after, v) = node;
    prevOf(next, v) = node;
}

void Vc2UpdateQueue::unlink(uint32_t node, Vc2PartVoices& v) {
    const uint32_t next = nextOf(node, v), prev = prevOf(node, v);
    prevOf(next, v) = prev;
    nextOf(prev, v) = next;
    prevOf(node, v) = node;
    nextOf(node, v) = node;
}

void Vc2UpdateQueue::waitCurrent(Vc2QueueParts& parts) {
    parts.wait(q.lock);
    parts.signal(q.lock);
}

// The current node moved on to its next, when it is `node`
bool Vc2UpdateQueue::advance(uint32_t node, Vc2PartVoices& v) {
    if (q.current != node) return false;
    q.current = nextOf(node, v);
    return true;
}

void Vc2UpdateQueue::enqueue(uint32_t node, Vc2PartVoices& v, Vc2QueueParts& parts) {
    if (q.current == node) {
        waitCurrent(parts);
        unlink(node, v);
    } else if (nextOf(node, v) != node) {
        return;
    }
    link(node, kHeadAt, v);
    v.at(node).queued = 1;
    v.at(node).queue = 0;
}

void Vc2UpdateQueue::moveOn(uint32_t node, Vc2PartVoices& v, Vc2QueueParts& parts) {
    if (advance(node, v)) parts.post(q.post);
    unlink(node, v);
}

void Vc2UpdateQueue::passOn(uint32_t node, Vc2PartVoices& v, Vc2QueueParts& parts) {
    if (advance(node, v)) parts.post(q.post);
}

void Vc2UpdateQueue::takeOff(uint32_t node, Vc2PartVoices& v, Vc2QueueParts& parts) {
    if (advance(node, v)) waitCurrent(parts);
    unlink(node, v);
}

void Vc2UpdateQueue::wait(uint32_t node, Vc2QueueParts& parts) {
    if (q.current == node) waitCurrent(parts);
}

void Vc2UpdateQueue::waitAll(Vc2QueueParts& parts) { waitCurrent(parts); }

void Vc2UpdateQueue::moveTo(uint32_t node, uint32_t queue, Vc2PartVoices& v) {
    advance(node, v);
    unlink(node, v);
    link(node, kHeadAt + kQueueBytes * queue, v);
    v.at(node).queue = (uint8_t)queue;
}
