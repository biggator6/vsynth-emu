#pragma once
// xt_vc2hireso's parts (v214): the formant warp's in Vc2HiresoCheck.cpp, the rate filters' in Vc2HiresoRateCheck.cpp,
// the analysis's (v215) in Vc2HiresoAnalysisCheck.cpp.  XT_VC2HIRESO_MAIN only.
#include "Vc2CheckRig.h"

// The rate filters' cases: each routine on a filter object in the interpreter's RAM against the port, block by block;
// the blocks that differ
long vc2HiresoRateCases(Vc2Fw& fw, const Vc2Run& run);
// The analysis's cases: its init, blocks and envelopes' updates on rom.4's globals against the port; the cases that
// differ
long vc2HiresoAnalysisCases(Vc2Fw& fw, const Vc2Run& run);
