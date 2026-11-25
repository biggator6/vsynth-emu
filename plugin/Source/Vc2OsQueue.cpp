// rom.4's OS message queue as a hub listener holds one (Vc2OsQueue.h, v355).
#include "Vc2OsQueue.h"

namespace {
constexpr uint8_t kOneWordAbove = 0x6f;   // [FW] the send's and the drain's test (0x89310fd2, 0x893110e4)

uint32_t after(const Vc2OsQueueWords& w, uint32_t a) { return a + 4 >= w.end ? w.start : a + 4; }

uint32_t& at(Vc2OsQueueRef q, uint32_t a) { return q.ring[(a - q.w.start) / 4]; }
} // namespace

int vc2QueueMessageWords(uint8_t status) { return status > kOneWordAbove ? 1 : 2; }

bool vc2QueueEmpty(const Vc2OsQueueWords& w) { return w.read == w.write; }

bool vc2QueuePut(Vc2OsQueueRef q, uint32_t message, uint32_t word) {
    const uint32_t first = after(q.w, q.w.write);
    if (vc2QueueMessageWords((uint8_t)message) == 1) {
        if (first == q.w.read) return false;
        at(q, first) = message;
        q.w.write = first;
        return true;
    }
    const uint32_t second = after(q.w, first);
    if (first == q.w.read || second == q.w.read) return false;
    at(q, first) = message;
    at(q, second) = word;
    q.w.write = second;
    return true;
}

bool vc2QueueTake(Vc2OsQueueRef q, uint32_t& word) {
    if (vc2QueueEmpty(q.w)) return false;
    q.w.read = after(q.w, q.w.read);
    word = at(q, q.w.read);
    return true;
}
