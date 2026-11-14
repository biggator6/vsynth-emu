#pragma once
// vc2run's runs, one file each (vc2run.cpp has the table that names them).  Each takes the machine with rom.4 loaded
// and its own arguments (argv[0] is the run's name), prints what it found, and returns the program's exit status.
#include "vc2rom.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace vc2 {

inline constexpr uint32_t kVocoder = 0x89999758;    // [FW] the static vocoder object, 0x5a78 bytes
inline constexpr uint32_t kBuild = 0x893f0490;      // [FW] its builder (this): both engines' constructors
inline constexpr uint32_t kHireso = kVocoder + 0x24, kLegacy = kVocoder + 0x2d94;   // [FW] the engines in it
inline constexpr uint32_t kHiresoSize = 0x2d70, kLegacySize = 0x2ce4;

// An engine's method by vtable slot: a vtable entry is 12 bytes, the function at +8, slot k at vtable + 12 + 12k
uint32_t slotOf(Machine& m, uint32_t engine, int slot);
// A call with fr4 (and fr5) set, as the engines' float arguments are passed
bool runF(Machine& m, uint32_t addr, std::initializer_list<uint32_t> args, float fr4, float fr5 = 0);
// A file's bytes, or empty when it can't be read
std::vector<uint8_t> fileContents(const char* path);
// A run's argument as a whole decimal number, or nothing when it isn't one
std::optional<int> intArg(const char* s);
// Every stub into the kernel (rom.4's 0x89301000…0x89302000, each a literal into 0x88000000…, which isn't mapped) answered
// with 0: the effect driver's waits and its OS calls; how many
int answerKernel(Machine& m);
// The vocoder module's static initializers, the init table's entries 0x898c609c…0x898c6108 (the carriers' rates, the
// mic's globals, the pitch tracker's setters, C4's period, …); false on a fault
bool initVocoderModule(Machine& m);

int boot(Machine& m, int argc, char** argv);
int warp(Machine& m, int argc, char** argv);
int hold(Machine& m, int argc, char** argv);
int growl(Machine& m, int argc, char** argv);
int kind6(Machine& m, int argc, char** argv);
int ratio(Machine& m, int argc, char** argv);
int lfo(Machine& m, int argc, char** argv);
int env(Machine& m, int argc, char** argv);
int cosm(Machine& m, int argc, char** argv);
int osc(Machine& m, int argc, char** argv);
int analog(Machine& m, int argc, char** argv);
int band(Machine& m, int argc, char** argv);
int solo(Machine& m, int argc, char** argv);
int lite(Machine& m, int argc, char** argv);
int tva(Machine& m, int argc, char** argv);
int patches(Machine& m, int argc, char** argv);
int track(Machine& m, int argc, char** argv);
int vocosc(Machine& m, int argc, char** argv);
int autonote(Machine& m, int argc, char** argv);
int mixer(Machine& m, int argc, char** argv);
int typed(Machine& m, int argc, char** argv);
int fxprog(Machine& m, int argc, char** argv);
int fxroute(Machine& m, int argc, char** argv);
int fxreverb(Machine& m, int argc, char** argv);
int splits(Machine& m, int argc, char** argv);
int splitbytes(Machine& m, int argc, char** argv);

} // namespace vc2
