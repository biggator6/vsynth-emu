#pragma once
// xt_vc2hireso's parts (v214): the formant warp's in Vc2HiresoCheck.cpp, the rate filters' in Vc2HiresoRateCheck.cpp,
// the analysis's (v215) in Vc2HiresoAnalysisCheck.cpp, the object's (v216) in Vc2HiresoObjectCheck.cpp.
// XT_VC2HIRESO_MAIN only.
#include "Vc2CheckRig.h"

class Vc2HiresoAnalysis;
class Vc2Unvoiced;

// The rate filters' cases: each routine on a filter object in the interpreter's RAM against the port, block by block;
// the blocks that differ
long vc2HiresoRateCases(Vc2Fw& fw, const Vc2Run& run);
// The analysis's cases: its init, blocks and envelopes' updates on rom.4's globals against the port; the cases that
// differ
long vc2HiresoAnalysisCases(Vc2Fw& fw, const Vc2Run& run);
// The object's cases (v216): its constructor once, then each case its reset and slot 1's blocks with the settings, the
// pedal and slot 4 between them, on an object in the interpreter's RAM against the port; the cases that differ
long vc2HiresoObjectCases(Vc2Fw& fw, const Vc2Run& run);

// The analysis's state, the mic at fs/2 and fs/4 and its decimators' states against rom.4's; empty when they are the
// same
std::string vc2HiresoAnalysisDifference(Vc2Fw& fw, const Vc2HiresoAnalysis& a);
// rom.4's analysis's globals and its decimators zeroed, as at the start: what no init clears (the rows, the
// decimators) as a new object meets it
void vc2HiresoAnalysisZero(Vc2Fw& fw);
// A block's flag for the analysis's gate: the last one's, flipped one block in six, into every word of both input
// stages' rings (rom.4's and `stage`'s), so 0x893ed3e0 and Vc2Unvoiced::voiced say voiced exactly when it is 0; the flag
bool vc2HiresoFlag(Vc2Fw& fw, Vc2Unvoiced& stage, std::mt19937& rng, bool unvoiced);
