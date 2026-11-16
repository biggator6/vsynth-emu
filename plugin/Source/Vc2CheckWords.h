#pragma once
// The VC-2 ports' words against rom.4's, for any of their checks (v219, moved out of the checks that had them, so the
// vocoder object's check can hold every part of the vocoder to rom.4's at once): the input and output stages'
// (xt_vc2in's), LEGACY's (xt_vc2legacy's) and HIRESO's (xt_vc2hireso's).  Each says where the first word that differs
// is, or nothing.  The checks' builds only.
#include "Vc2CheckRig.h"
#include "Vc2Hireso.h"
#include "Vc2Legacy.h"
#include "Vc2Unvoiced.h"
#include <string>

inline constexpr uint32_t kVc2MicRingAt = 0x8a0f73dc, kVc2FlagsAt = 0x8a0f87dc;   // [FW] the input stage's two rings
inline constexpr uint32_t kVc2HiresoSettingWordsAt = 0x2cb4;   // [FW] HIRESO's settings' words, in its object

// The input and output stages' region (0x899d9978…), the flags' ring, and rom.4's two .data words: the level as it
// glides and the allpasses' index
std::string vc2UnvoicedDifference(Vc2Fw& fw, const Vc2Unvoiced& port);
// The memory the blocks show only through the frames: the mic ring and both allpasses' buffers
std::string vc2UnvoicedBuffersDifference(Vc2Fw& fw, const Vc2Unvoiced& port);
// LEGACY's analysis's state (0x8a1bd9d4…) and its two .data words, the dump time and the level
std::string vc2LegacyStateDifference(Vc2Fw& fw, const Vc2LegacyAnalysis& port);
// LEGACY's object at `at` in rom.4's memory (its words from +0x004) and its analysis
std::string vc2LegacyDifference(Vc2Fw& fw, uint32_t at, const Vc2Legacy& engine);
// HIRESO's analysis's state (0x8a1bbd2c…), the mic at fs/2 and fs/4 and its decimators' states
std::string vc2HiresoAnalysisDifference(Vc2Fw& fw, const Vc2HiresoAnalysis& a);
// HIRESO's analysis's globals and its decimators zeroed, as at the start: what no init clears (the rows, the
// decimators) as a new object meets it
void vc2HiresoAnalysisZero(Vc2Fw& fw);
// HIRESO's object at `at`: its words, its rate filters' states and the words their tables point at, its settings'
// words, its analysis, and slot 7's level through rom.4's routine
std::string vc2HiresoDifference(Vc2Fw& fw, uint32_t at, const Vc2Hireso& e);
