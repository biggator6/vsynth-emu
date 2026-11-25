#pragma once
// rom.4's OS message queue as a hub listener holds one (v355), word for word: the notes' task's (0x8998190c, its ring
// 0x89981940, 0x200 bytes) and the OS MIDI task's (0x8995cca0, its ring 0x8995ccd4, 0x1000 bytes). The listener's node
// on the hub's ring and its class, then the queue (the MIDI library's, 0x89310c00…0x89311180): its ring's start, end,
// read and write (the last word read and the last written: empty at read == write, a word put at write + 4 and taken at
// read + 4, each wrapping at the end to the start), its waiters' records, its owner (the task a put posts), the drain's
// flag and its handler's class. A message is one word with a status above 0x6f, two with one below (the parameters'
// messages, a SysEx). The puts (0x89310c00, 0x89310c70) and the take (0x89310d00) here; the send's wait for room, the
// wait for empty and the drain's waking of its waiters are the tasks' (Vc2SynthOsMidi). HARDWARE_FIRMWARE.md, "The
// part's tasks on the kernel".
#include <array>
#include <cstdint>
#include <span>

// A node of one of the OS's rings (XtRing.h's), by address: the one before and the one after
struct Vc2RingNode {
    uint32_t prev = 0, next = 0;
};

// A listener with no queue: its node, its class (the MIDI out's 0x899598a4, the clock task's 0x8995cc08)
struct Vc2RingListener {
    Vc2RingNode node {};
    uint32_t klass = 0;
};

struct Vc2OsQueueWords {
    Vc2RingNode node {};                               // +0x00: its node on the hub's ring
    uint32_t klass = 0;                                // +0x08
    uint32_t start = 0, end = 0, read = 0, write = 0;  // +0x0c: the queue's ring, by address
    uint32_t unread1c = 0, unread20 = 0;               // +0x1c, +0x20: the queue's +0x10 (its other path's), +0x14
    uint32_t waiters = 0;                              // +0x24: the queue's +0x18, its waiters' records, on their stacks
    uint32_t owner = 0;                                // +0x28: the queue's +0x1c, the task a put posts
    uint8_t draining = 0;                              // +0x2c: the queue's +0x20, set while its drain runs
    std::array<uint8_t, 3> pad2d {};
    uint32_t handler = 0;                              // +0x30: the queue's +0x24, its handler object's class
};
static_assert(sizeof(Vc2OsQueueWords) == 0x34, "rom.4's layout, a hub listener with its queue");

// A queue's words and its ring of `Words`, the ring right after the words in rom.4's memory
template <size_t Words>
struct Vc2OsQueue {
    Vc2OsQueueWords w {};
    std::array<uint32_t, Words> ring {};
};

// A queue as its routines take it: its words and its ring
struct Vc2OsQueueRef {
    Vc2OsQueueWords& w;
    std::span<uint32_t> ring;
    template <size_t Words>
    Vc2OsQueueRef(Vc2OsQueue<Words>& q) : w(q.w), ring(q.ring) {}   // NOLINT: a queue is its ref
};

// The queue built as its row builds it (0x89310f40): at `at`, its ring empty, its class, owner and handler's class
template <size_t Words>
void vc2QueueBuild(Vc2OsQueue<Words>& q, uint32_t at, uint32_t klass, uint32_t owner, uint32_t handler) {
    q = Vc2OsQueue<Words> {};
    q.w.klass = klass;
    q.w.start = q.w.read = q.w.write = at + (uint32_t)sizeof(Vc2OsQueueWords);
    q.w.end = q.w.start + (uint32_t)(Words * 4);
    q.w.owner = owner;
    q.w.handler = handler;
}

int vc2QueueMessageWords(uint8_t status);   // 1 above 0x6f, else 2
bool vc2QueueEmpty(const Vc2OsQueueWords& w);
// A message's words put (0x89310c00, 0x89310c70): false, nothing put, when they don't fit
bool vc2QueuePut(Vc2OsQueueRef q, uint32_t message, uint32_t word);
// The next word taken (0x89310d00): false when none
bool vc2QueueTake(Vc2OsQueueRef q, uint32_t& word);
