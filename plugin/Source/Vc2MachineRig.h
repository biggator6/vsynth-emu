#pragma once
// The XT's part on rom.1's kernel in one interpreter (v355), for xt_vc2synth's whole part: rom.4 (Vc2Fw) with rom.1
// beside it (vc2LoadKernel), the pages its drivers touch mapped as RAM, and nothing of the kernel answered but its
// hardware (TMU0's set-up and acknowledgement, the watchdog's start and stop). Its context switch runs as rom.1's own,
// with three instructions the interpreter hasn't done in hooks: the save's `stc.l sr,@-r15` (0x88010fe0) and the loads'
// `ldc.l @r15+,spc; ldc.l @r15+,ssr; rte` (0x88010fd8, 0x88011028). rom.4's main's start (the kernel's objects' table
// first, then the rows, the MIDI library's among them); rom.4's start makes the part's four tasks, the MIDI input task
// and the OS MIDI task (entries 1 and 6), each running its own prologue to its first wait, and entry 12's TCB as the
// front's stand-in, whose code is a hook: each message given it sent as the front sends one (0x89314d60). The rest of
// rom.4's tasks and the kernel's own but its idle leave their queues; the rig's calls are the idle task's, so a call
// returns when every task waits again. Then the front's MIDI on (0x8931a590): the hub on the MIDI IN ring. Outside
// events: TMU0 (0x8800ea30), the DMA's two interrupts (their handlers 6 and 7, as the audio task's prologue set them),
// a channel message on source 1 or 2 its bytes into rom.4's receive parser (0x893123b0, MIDI IN's and USB's objects),
// each the interrupt's, then the watchdog's put-off dispatch when started; any other message the front's. The OS's own
// work outside the part answers 0: its timer queue, the routines the 0x40 and 0x01 messages run, the MIDI clock out.
// HARDWARE_FIRMWARE.md, "The part's tasks on the kernel". xt_vc2synth only.
#include "Vc2CheckRig.h"
#include "Vc2MidiDispatch.h"
#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

struct Vc2MachineRig {
    // [FW] rom.1's: its system words (curTask first), its idle task's TCB, the application's state word and the
    // interrupt handlers' table
    static constexpr uint32_t kSystem = 0x880f3ce0u, kIdle = 0x880f45c4u, kAppState = 0x880f21b4u;
    static constexpr uint32_t kHandlers = 0x880f3b3cu;
    // [FW] rom.4's task table's first and last entries' words; the tasks the rig runs: the part's four (audio, clock,
    // notes, update), the MIDI input task (entry 1), the OS MIDI task (entry 6) and the front's stand-in (entry 12)
    static constexpr uint32_t kTableFirst = 0x8950cd60u, kTableLast = 0x8950cd64u, kEntryBytes = 20;
    static constexpr std::array<uint32_t, 7> kTasks { 0x8994fd5cu, 0x8994fdf8u, 0x8994fec8u, 0x8994ff64u,
                                                      0x8994fd90u, 0x8994fe94u, 0x8994ffccu };
    // [FW] the input's and the output's DMA buffers, a block's bytes in each
    static constexpr uint32_t kInputBuffers = 0x8cc3f800u, kOutputBuffers = 0x8cc3f000u, kBufferShift = 10;

    Vc2Fw fw;
    std::vector<uint32_t> own;   // the kernel's own tasks' TCBs
    bool watchdog = false;       // started and not stopped
    long loads = 0;              // contexts loaded (each rte)
    // The kernel's services rom.4 reaches that the port's model holds as changing nothing, each counted where it would
    // change something: an event set on a task suspended (the update task suspends the notes' task around C's refresh,
    // 0x893018e0 and 0x893018f0); a semaphore's wait while another task holds it (the pool's lock, the update queue's);
    // a task's restart (0x893018a0, the update queue's move on, given its task); and the suspends and waits seen
    long suspendedSets = 0, heldWaits = 0, restarts = 0, suspends = 0, semaphoreWaits = 0;
    // A queue's sender waiting (0x89310e70): for room in the notes' ring, for the OS MIDI task's queue to empty; a
    // receive object's marker (0x89311e90, 0x89311f40) by its code (0 a receive error, 1 active sensing's end, 2 the
    // ring full, 4 USB-MIDI Thru changed)
    long roomWaits = 0, emptyWaits = 0;
    std::array<long, 5> markers {};

    // rom.4 and rom.1 from beside it, the pages, the hooks; false, `why` saying so, when they can't be
    bool load(const char* rom4, std::string& why);
    // rom.4's main's first call, before the rows: its tables of the kernel's semaphores and cyclic records
    std::string prepare();
    // The kernel's start, the application's state on, rom.4's start: the tasks run to their first waits; the MIDI on
    std::string begin();
    std::string tick();                                     // TMU0's interrupt
    std::string input(const int32_t* frames);               // the input's DMA interrupt, its buffer given `frames`
    std::string output();                                   // the output's DMA interrupt
    std::string message(const Vc2MidiMessage& m, uint32_t word);
    // A routine run as the idle task's, then the watchdog's interrupt when started (an interrupt's end)
    std::string idleCall(uint32_t f, std::initializer_list<uint32_t> args, const char* what);
    uint32_t word(uint32_t at) { return fw.cpu.rd32(at); }

private:
    std::string interruptEnd();
    std::string handler(int index, const char* what);
    void countServices();
    void frontStandIn();
    std::vector<uint32_t> suspended_;   // the TCBs suspended now
    std::deque<std::pair<Vc2MidiMessage, uint32_t>> front_;   // the messages given the front, not yet sent
};
