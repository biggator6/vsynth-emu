#pragma once
// xt_vc2cosm's parts (v235): the Growl's (Vc2CosmCheck.cpp, v234, and main) and the converters' frame's
// (Vc2CosmFrameCheck.cpp).  XT_VC2COSM_MAIN only.
#include "Vc2CheckRig.h"

// The frame's cases on the run's seed: rom.4's dispatcher and the port's on the same inputs; how many differ
long vc2CosmFrameCases(Vc2Fw& fw, const Vc2Run& run);

// The tables both parts read: COSM's pool words and the Matrix's limits
bool vc2LoadCosmAndMatrix(const char* path);
