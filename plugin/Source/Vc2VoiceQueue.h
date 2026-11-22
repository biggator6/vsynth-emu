#pragma once
// The VC-2 part's update queue (v279, step 4.3's seventh version): rom.4's one queue of 36 bytes at 0x89989cc4, word
// for word: a head node at +4 (a voice's first five words' layout), the node being run at +0x18 and two OS objects at
// +0x1c and +0x20.  Its constructor 0x89394f60 in place and the static initializer 0x89395280 (an array of one), a
// node's link after another 0x89394f10 and unlink 0x89394f20, the enqueue 0x89394fa0 (at the front), the move on
// 0x89395040 and the pass on 0x893950b0, the take off 0x89395120, the waits 0x893951b0 and 0x893951d0, and the move to
// another queue 0x89395210.  The nodes are the voices' (their +0x00…+0x13) or the head, by address, and from v312 one
// node besides, the static M's, given to the queue by its address (`setNode`).  The OS's wait, signal and post through
// Vc2QueueParts; the interrupts' masking around them changes no memory.  Bit for bit: xt_vc2voices' fourth part, the
// static M's node on it its seventh.  HARDWARE_FIRMWARE.md, "The VC-2 part's voices' virtuals and update queue".
#include "Vc2PartVoices.h"
#include "Vc2QueueNode.h"

struct Vc2QueueWords {
    uint32_t unread = 0;                // +0x00
    uint32_t next = 0;                  // +0x04: the head's links
    uint32_t prev = 0;                  // +0x08
    uint32_t queued = 0;                // +0x0c
    uint8_t byte = 0;                   // +0x10
    std::array<uint8_t, 3> pad {};      // +0x11
    uint32_t vtable = 0;                // +0x14
    uint32_t current = 0;               // +0x18: the node being run
    uint32_t post = 0;                  // +0x1c: an OS object, posted when the current node moves on
    uint32_t lock = 0;                  // +0x20: an OS object, waited on and signalled around the current node
};
static_assert(sizeof(Vc2QueueWords) == 0x24, "rom.4's layout, the update queue");

// The OS's calls a queue makes, given an object's word
class Vc2QueueParts {
public:
    virtual ~Vc2QueueParts() = default;
    virtual void wait(uint32_t object) = 0;     // 0x893018b0
    virtual void signal(uint32_t object) = 0;   // 0x893018c0
    virtual void post(uint32_t object) = 0;     // 0x893018a0
};

// A node's words on a queue, wherever its layout keeps them
struct Vc2QueueSlots {
    uint32_t& next;
    uint32_t& prev;
    uint32_t& queued;
    uint8_t& queue;
};

class Vc2UpdateQueue {
public:
    static constexpr uint32_t kAt = 0x89989cc4, kHeadAt = kAt + 4;    // [FW] the queue, its head
    static constexpr uint32_t kHeadVtable = 0x898cd358;               // [FW] the head's class

    void construct();                                                    // 0x89394f60 on the queue given
    void staticInit();                                                   // 0x89395280
    void link(uint32_t node, uint32_t after, Vc2PartVoices& v);          // 0x89394f10
    void unlink(uint32_t node, Vc2PartVoices& v);                        // 0x89394f20
    void enqueue(uint32_t node, Vc2PartVoices& v, Vc2QueueParts& parts);    // 0x89394fa0
    void moveOn(uint32_t node, Vc2PartVoices& v, Vc2QueueParts& parts);     // 0x89395040
    void passOn(uint32_t node, Vc2PartVoices& v, Vc2QueueParts& parts);     // 0x893950b0
    void takeOff(uint32_t node, Vc2PartVoices& v, Vc2QueueParts& parts);    // 0x89395120
    void wait(uint32_t node, Vc2QueueParts& parts);                         // 0x893951b0
    void waitAll(Vc2QueueParts& parts);                                     // 0x893951d0
    void moveTo(uint32_t node, uint32_t queue, Vc2PartVoices& v);           // 0x89395210, queue 0 the only one

    Vc2QueueWords q {};

    // The one node besides the voices' and the head, at its address (v312: the static M's); none until it is given
    void setNode(uint32_t at, Vc2QueueNode& node);

    // A node's words, the head's, the node given's or a voice's (v312); its two links alone (public from v310, the
    // update task's walk's)
    Vc2QueueSlots slotsOf(uint32_t node, Vc2PartVoices& v);
    uint32_t& nextOf(uint32_t node, Vc2PartVoices& v);
    uint32_t& prevOf(uint32_t node, Vc2PartVoices& v);

private:
    void waitCurrent(Vc2QueueParts& parts);
    bool advance(uint32_t node, Vc2PartVoices& v);

    uint32_t givenAt_ = 0;
    Vc2QueueNode* given_ = nullptr;
};
