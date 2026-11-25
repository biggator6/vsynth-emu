#pragma once
// The part's tasks on rom.1's kernel (v355): rom.4's audio, clock, notes and update tasks, the MIDI input task, the OS
// MIDI task and the front (the rig's stand-in on entry 12) on Vc2Kernel at their levels and quanta, with an idle task
// below them, as on an XT. A task's first run is what of its prologue is the part's, then each run is a pass of its
// loop on what its wait took, the pass saying what it waits on next (its loop's mask, or 0x8000 where a sender waits
// inside its tell, its place kept). A post from a pass that switches runs the woken tasks' passes until the poster runs
// again, as rom.1's switch does; an interrupt (the 1 ms tick, a block's DMA, a MIDI byte) is taken with the idle task
// running, every task waiting, and the tasks it wakes run before the next. A wait inside a pass (the notes' task's on a
// fade) lets time run on until its task runs again (HARDWARE_FIRMWARE.md, "The part's tasks on the kernel").
#include "Vc2Kernel.h"
#include <array>
#include <cstdint>
#include <utility>

// What the OS model runs of the part
struct Vc2OsParts {
    virtual ~Vc2OsParts() = default;
    virtual void prologue(int task) = 0;                  // the part's of a task's start, before its loop
    virtual uint16_t pass(int task, uint32_t events) = 0; // its loop body on what its wait took; what it waits on next
    virtual void cyclic(int handler) = 0;                 // a due cyclic handler's (the update task's 0x893952a0, …)
    virtual void appTick() = 0;                           // TMU0's application handler's (the clock's ms)
    virtual void advance() = 0;                           // time on to the next interrupt, a task waiting in its pass
};

class Vc2SynthOs {
public:
    enum Task : int { kAudio, kClock, kNotes, kUpdate, kInput, kOsMidi, kFront, kIdle, kTasks };
    // [FW] the tasks' event objects (rom.4's TCBs, its task table's entries 0, 3, 7, 10, 1, 6 and 12), their levels
    // (0x8950cd44) and quanta (the entries' +0xc), and their loops' waits' masks (the front's the rig's); the idle the
    // kernel's own, below them all
    static constexpr std::array<uint32_t, kIdle> kObjects { 0x8994fd5cu, 0x8994fdf8u, 0x8994fec8u, 0x8994ff64u,
                                                            0x8994fd90u, 0x8994fe94u, 0x8994ffccu };
    static constexpr std::array<std::pair<int, uint8_t>, kTasks> kLevels { { { 0, 0 }, { 3, 0 }, { 14, 0 }, { 16, 8 },
                                                                             { 1, 0 }, { 12, 0 }, { 27, 0 },
                                                                             { 31, 0 } } };
    static constexpr std::array<uint16_t, kIdle> kMasks { 7, 0x47, 0x3fe, 3, 0xe, 2, 1 };
    // The senders (the MIDI input task, the front), whose passes keep their place: a post of theirs that switches ends
    // the pass there, the task ready, and its next run goes on from it, so no pass of theirs is below another task's
    static constexpr std::array<bool, kTasks> kGivesWay { false, false, false, false, true, false, true, false };

    Vc2Kernel kernel;
    explicit Vc2SynthOs(Vc2OsParts& parts) : parts_(parts) {}
    // The tasks made as rom.4's start makes them, then the first dispatch: each task's first run to its wait
    void start();
    void post(uint32_t object, uint32_t flags);    // an event set from the running task (0x89301830)
    void raise(uint32_t object, uint32_t flags);   // from an interrupt (0x89301840)
    // TMU0's interrupt: the kernel's tick, its due cyclic handlers run, then the application's handler
    void tick();
    // An interrupt's end: the watchdog's interrupt when started, then the tasks woken, to the idle task
    void interruptEnd();
    // The dispatch locked out (0x89301880) and let in again (0x89301890), the woken then run until the task that let
    // it in runs again
    void lockOut() { kernel.lockOut(); }
    void unlockOut();
    void waitInPass(uint16_t mask);                // the running task's wait inside its pass (0x893017f0)
    // A wait inside a pass with no event left to take: the model stuck, its passes then unwinding with no more posts,
    // waits or runs on the kernel
    void giveUp() { stuck_ = true; }
    // Whether a task was resumed that waits inside a pass below the running one's, which one thread can't run (rom.4's
    // order of work never asks it: HARDWARE_FIRMWARE.md)
    bool stuck() const { return stuck_; }
    int taskOf(uint32_t object) const;
    // Whether a sender's post switched, its pass to end there (kGivesWay)
    bool gaveWay(int task) const { return gaveWay_[(size_t)task]; }

private:
    Vc2OsParts& parts_;
    std::array<uint16_t, kTasks> took_ {};
    std::array<bool, kTasks> started_ {}, inPass_ {}, gaveWay_ {};
    bool stuck_ = false;
    void took(const Vc2Kernel::Switch& s);
    void runUntil(int task);
    void run(int task);
};
