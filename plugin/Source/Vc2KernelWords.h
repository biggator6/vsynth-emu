#pragma once
// rom.1's kernel's words as each side gives them, for the checks that hold Vc2Kernel against rom.1's own (v355, moved
// out of xt_vc2kernel's Vc2KernelCheck.cpp, v353, as they were): each task's TCB words, readyQueue, the system's words
// and the watchdog, the 16 cyclic records; then the first that differs, said. How a side names its tasks and handlers
// is given (the kernel check's TCBs in its own RAM; the whole part's, rom.4's own and the kernel's idle).
#include "Sh4Interp.h"
#include "Vc2Kernel.h"
#include <cstdint>
#include <string>
#include <vector>

// One word of the state, by what it is and whose
struct Vc2KernelWord {
    const char* what;
    int whose;
    int64_t value;
    bool operator==(const Vc2KernelWord&) const = default;
};

// A side's names: a task's TCB by its number, an address as a task's number (-1 for none), a cyclic record's handler
// as its number (-1 for none)
struct Vc2KernelNames {
    uint32_t (*tcb)(int task);
    int64_t (*task)(uint32_t at);
    int64_t (*handler)(uint32_t at);
};

// rom.1's words in the interpreter, for `tasks` tasks by `names`; freezTask's 0xffffffff is none
std::vector<Vc2KernelWord> vc2KernelTheirs(Sh4& c, int tasks, const Vc2KernelNames& names, bool watchdog);
std::vector<Vc2KernelWord> vc2KernelOurs(const Vc2Kernel& k);
// The first word that differs, "what whose: rom.1's a, the port's b"; empty when none does
std::string vc2KernelDifference(const std::vector<Vc2KernelWord>& theirs, const std::vector<Vc2KernelWord>& ours);
