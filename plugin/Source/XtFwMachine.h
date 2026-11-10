#pragma once
// The machine each of XtFirmware's routines runs on (moved from XtFirmware.cpp in v175): rom.2 in the interpreter, its
// RAM as research/ghidra_scripts' emulator scripts lay it out, the OS calls a routine makes (malloc, calloc, free, the
// OS's init, the UI) answered by hooks, and how long a call may run.  Each call is allowed kCallSteps, and
// kStepsPerSample more for every sample of the wave its routine was given, where every call used to be allowed 20e9
// steps, 73 s at the interpreter's 275 M steps/s.  So a routine that loops gives up in under 0.2 s on the message
// thread (a patch's store, a .vpw's blocks, the tables: no wave) and in proportion to its wave in an encode
// (.claude/fix-plan.md, section 9; VSynthEmuStepTest).  research/tools/fwsteps.sh measures each kind of call against
// these.
#include "Sh4Interp.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace xtfw {

// The harness memory, as the emulator scripts use it
inline constexpr uint32_t kRam = 0x8C000000, kRamSize = 0x02000000;
inline constexpr uint32_t kStack = 0x8C0E0000, kEv = 0x8C0F0000, kL = 0x8C0F0000, kInfo = 0x8C0F0100;
inline constexpr uint32_t kSamp = 0x8C100000, kHeap = 0x8D000000;

// How long a call may run, in the interpreter's steps (fwsteps.sh 1, 2026-11-10: the disk image's 512 patches and 416
// preset waves, a project's 263 patches and 91 waves, the tables, and every preset wave encoded and imported)
inline constexpr uint64_t kCallSteps = 50'000'000;   // 97 times a patch's store, 0.51 M, the most a fixed call took
inline constexpr uint64_t kStepsPerSample = 40'000;  // 3.3 times a SOLO encode's 12,091 past that, the most of any wave

struct Machine {
    Sh4 cpu;
    uint32_t heap = kHeap;
    bool ok = false;
    // On the image at `rom`, each call allowed its steps for `samples` of a wave: what its routine works through,
    // frames by channels (an import's, its input and its output); none for a routine that does a fixed amount of work
    explicit Machine(const std::string& rom, std::size_t samples = 0);
};

// A machine with `frames` of a wave of `channels` (1 or 2) from `pcm` at kSamp and `reserve` bytes free above them
// below the heap, each call allowed its steps for them: nullptr with no image, no wave, one that doesn't fit, or an
// image that doesn't load.  The SOLO and band encoders' and the band rebuild's.
std::unique_ptr<Machine> waveMachine(const std::string& rom, const int16_t* pcm, long frames, int channels,
                                     std::size_t reserve);

// Every machine made after this rounds to nearest, not to zero as the SH-4 does (xtFwForceNearest: XT_ROUND in a
// harness)
void roundNearest(bool on);

} // namespace xtfw
