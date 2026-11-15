#pragma once
// What xt_vc2legacy's two parts share (v211): the analysis's (Vc2LegacyCheck.cpp, v210) and the object's
// (Vc2LegacySynthCheck.cpp).  XT_VC2LEGACY_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2Legacy.h"
#include <bit>
#include <random>
#include <string>

inline constexpr uint32_t kVc2LegacyWords = 0x8a1bd9d4;                                   // [FW] the analysis's state
inline constexpr uint32_t kVc2LegacyDumpWord = 0x89940254, kVc2LegacyLevelWord = 0x89940258;   // [FW] its .data words
inline constexpr uint32_t kVc2LegacyBlockAt = 0x8C011000;   // the mic's block, in Vc2Fw's RAM
inline constexpr int kVc2LegacyBlocks = 400, kVc2LegacyFadeEvery = 8;   // a case's blocks; every eighth case fades

// A getter's float result, as rom.4 leaves it in fr0, against the port's
inline bool vc2SameAsFr0(const Vc2Fw& fw, float mine) { return fw.cpu.fb[0][0] == std::bit_cast<uint32_t>(mine); }

// The Hold pedal's 0…127: its two ends most of the time, as a pedal gives; 64 and 63, where the hold's branches part
int vc2DrawController(std::mt19937& rng);
// A dump time: its ends most often (127 and 126 put the pedal's hold either side of the freeze at 0.99, 0 holds nothing)
float vc2DrawDump(std::mt19937& rng);
// The analysis's state and its two .data words against rom.4's; empty when they are the same
std::string vc2LegacyStateDifference(Vc2Fw& fw, const Vc2LegacyAnalysis& port);
bool vc2LegacyFirst();   // XT_VC2LEGACY_FIRST: say where the first case to differ first does
// The object's cases (Vc2LegacySynthCheck.cpp), from the analysis's state as its cases left it; the cases that differ
long vc2LegacySynthesisCases(Vc2Fw& fw, const Vc2LegacyAnalysis& carried, const Vc2Run& run);
