#pragma once
// vc2run's runs, one file each (vc2run.cpp has the table that names them).  Each takes the machine with rom.4 loaded
// and its own arguments (argv[0] is the run's name), prints what it found, and returns the program's exit status.
#include "vc2rom.h"

namespace vc2 {

inline constexpr uint32_t kVocoder = 0x89999758;    // [FW] the static vocoder object, 0x5a78 bytes
inline constexpr uint32_t kBuild = 0x893f0490;      // [FW] its builder (this): both engines' constructors
inline constexpr uint32_t kHireso = kVocoder + 0x24, kLegacy = kVocoder + 0x2d94;   // [FW] the engines in it
inline constexpr uint32_t kHiresoSize = 0x2d70, kLegacySize = 0x2ce4;

// An engine's method by vtable slot: a vtable entry is 12 bytes, the function at +8, slot k at vtable + 12 + 12k
uint32_t slotOf(Machine& m, uint32_t engine, int slot);
// A call with fr4 (and fr5) set, as the engines' float arguments are passed
bool runF(Machine& m, uint32_t addr, std::initializer_list<uint32_t> args, float fr4, float fr5 = 0);

int boot(Machine& m, int argc, char** argv);
int warp(Machine& m, int argc, char** argv);
int hold(Machine& m, int argc, char** argv);
int growl(Machine& m, int argc, char** argv);
int kind6(Machine& m, int argc, char** argv);
int ratio(Machine& m, int argc, char** argv);

} // namespace vc2
