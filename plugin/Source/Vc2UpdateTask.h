#pragma once
// The VC-2 part's update task, its pass (v310, step 4.3's thirty-eighth version, item 10's fifteenth part): rom.4's
// loop body 0x89395316…0x89395414 in the task 0x893952b0 (its object 0x8994ff64), given the events its wait
// (0x893017f0, mask 3) answers, in rom.4's order: 0x2, the timer's (0x893952a0 counts its ticks at 0x8993d080): with
// ticks elapsed since the last pass, each node on the update queue (v279's, 0x89989cc4), by its +4 link from the
// head's, made the queue's current node and given its vtable's (+0x10) +0x0c virtual (a voice's update, v279's) with
// the time struct, under the queue's lock (+0x20, the OS's wait and signal), its +8 the answer's low half, and taken off
// (unlinked, the current node its +0 link) when that is 0 and it is still linked; then the ticks since the last pass
// measured into the time struct 0x8993d084 (its count, as a half, and its last).  0x1: the pitch source 0x89394a60
// (on 0x89989748) and the voices' update 0x893b16a0.  The interrupts' masking around the walk changes no memory.  The
// calls out through Vc2UpdateTaskParts.  The queue's nodes the voices' and, from v312, the static M's (its words through
// the queue's slotsOf; its update M's init 0x893a2ee0 on the static M).  Not in the sound yet.  Bit for bit:
// xt_vc2voices' sixth and seventh parts.  HARDWARE_FIRMWARE.md, "The VC-2 part's update task".
#include "Vc2VoiceQueue.h"
#include <cstdint>

// one-off: rom.4's own layout; a declaration, not a job to template
// [FW] the time struct a node's update is given, at 0x8993d084
struct Vc2TaskTime {
    int16_t elapsed = 0;   // +0: the ticks since the last pass
    uint16_t unread2 = 0;
    uint32_t last = 0;     // +4: the tick count at the last pass
};

class Vc2UpdateTaskParts : public Vc2QueueParts {
public:
    // A node's vtable's (+0x10) +0x0c, given the time struct's address; its answer
    virtual uint32_t nodeUpdate(uint32_t node, uint32_t time) = 0;
    virtual void pitchSource() = 0;    // 0x89394a60 on 0x89989748
    virtual void voicesUpdate() = 0;   // 0x893b16a0
};

struct Vc2UpdateTaskWorld {
    Vc2UpdateQueue& queue;
    Vc2PartVoices& voices;
    Vc2TaskTime& time;      // 0x8993d084
    const uint32_t& ticks;  // 0x8993d080: the timer's ticks
    Vc2UpdateTaskParts& parts;
};

void vc2UpdateTaskPass(Vc2UpdateTaskWorld& w, uint32_t events);
