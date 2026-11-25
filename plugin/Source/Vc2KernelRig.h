#pragma once
// rom.1, the kernel, in the interpreter for the VC-2's checks (v353): its image at 0x88000000 with .data and .bss as its
// C start-up lays them out, read at run time from beside rom.4 (firmware/unpacked's V-SynthXT.rom.1.bin); the hardware
// it touches hooked (TMU0's set-up and acknowledgement, the watchdog's start and stop); its context switch hooked, so a
// service returns at once and says which task it switched to (HARDWARE_FIRMWARE.md, "The kernel's event services");
// and the check's tasks' TCBs in RAM it maps. xt_vc2kernel's build only.
#include "Sh4Interp.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// v355: rom.1 from beside rom.4 into an interpreter (its image, .data and .bss by its start-up's words, its RAM mapped);
// its own tasks' TCBs into `own`; false, `why` saying so, when it can't be
bool vc2LoadKernel(Sh4& cpu, const char* rom4, std::vector<uint32_t>& own, std::string& why);
// A hook that answers 0 and returns (v355: the two rigs')
inline void vc2AnswerNothing(Sh4& c) { c.ret(0); }

struct Vc2KernelRig {
    // [FW] rom.1's words and routines (research/tools/kernelmap.py and HARDWARE_FIRMWARE.md, "rom.1, the kernel")
    static constexpr uint32_t kBase = 0x88000000u;       // its reset code's VBR
    static constexpr uint32_t kSystem = 0x880f3ce0u;     // curTask, sysTime, freezTask, lockFlag, alarmTop, roundLevel
    static constexpr uint32_t kReadyQueue = 0x880f3cf8u; // readyQueue[32]
    static constexpr uint32_t kCyclics = 0x880f220cu;    // the 16 cyclic records' addresses
    static constexpr uint32_t kOwnTasks = 0x88061514u, kOwnTasksEnd = 0x88061578u;   // its own tasks' entries
    static constexpr uint32_t kStart = 0x88010ba0u;      // the kernel's start: its tasks, queues, records, words
    static constexpr uint32_t kReset = 0x8800ff50u;      // a TCB reset by its entry, its context made
    static constexpr uint32_t kMakeReady = 0x8800fa40u, kUnready = 0x8800fa70u;
    static constexpr uint32_t kFirstDispatch = 0x88010ef0u;
    static constexpr uint32_t kWait = 0x8800ffc0u, kSet = 0x880100a0u, kRaise = 0x88010190u;
    static constexpr uint32_t kWatchdogIrq = 0x8800fb60u, kTick = 0x8800fdf0u, kInterval = 0x880101e0u;
    static constexpr uint32_t kCyclicMake = 0x88010810u, kCyclicSwitch = 0x88010890u;
    static constexpr uint32_t kHold = 0x88010250u, kRelease = 0x880102d0u, kLockOut = 0x88010290u;
    static constexpr uint32_t kUnlockOut = 0x88010320u;
    static constexpr uint32_t kTcbBytes = 0x40;          // the check's TCBs' spacing (a TCB is 0x34 bytes)
    // The check's RAM: the stack its calls run on, the TCBs, their entries and stacks, a cyclic record's spec, and the
    // handlers' addresses (hooks, never code)
    static constexpr uint32_t kRam = 0x8C000000u, kRamBytes = 0x00100000u, kSp = 0x8C0F0000u;
    static constexpr uint32_t kTcbs = 0x8C010000u, kEntries = 0x8C020000u, kStacks = 0x8C030000u;
    static constexpr uint32_t kSpec = 0x8C0F8000u, kHandlers = 0x8C0FA000u, kStackBytes = 0x400u;

    Sh4 cpu;
    std::vector<uint32_t> own;                           // the kernel's own tasks' TCBs
    std::vector<std::pair<uint32_t, uint32_t>> switches; // each switch's TCBs, from and to, since the last clear
    uint32_t answer = 0;                                 // what the last switch's task took, before its tail
    bool watchdog = false;                               // started and not stopped

    // rom.1 from beside rom.4 loaded and its own start run; false, `why` saying so, when it can't be
    bool load(const char* rom4, std::string& why);
    // The kernel's start again, its own tasks then waiting, and `tasks` (level, quantum) made and ready as rom.4's
    // start makes its own, then the first dispatch
    bool begin(const std::vector<std::pair<int, uint8_t>>& tasks);
    bool call(uint32_t f, std::initializer_list<uint32_t> args, uint32_t* r0 = nullptr);
    static uint32_t tcb(int t) { return kTcbs + kTcbBytes * (uint32_t)t; }
    static uint32_t handler(int h) { return kHandlers + 0x10u * (uint32_t)h; }
    // A TCB's address as the check's task, -1 for 0, -2 for one of the kernel's own or anything else
    static int task(uint32_t a);
    uint32_t cyclic(int k) { return cpu.rd32(kCyclics + 4u * (uint32_t)k); }
};
