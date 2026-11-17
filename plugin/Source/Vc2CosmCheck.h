#pragma once
// xt_vc2cosm's parts (v235): the Growl's (Vc2CosmCheck.cpp, v234, and main) and the converters' frame's
// (Vc2CosmFrameCheck.cpp, its draws in Vc2CosmFrameDraws.cpp, v237).  XT_VC2COSM_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2CosmInput.h"

// The frame's cases on the run's seed: rom.4's dispatcher and the port's on the same inputs; how many differ
long vc2CosmFrameCases(Vc2Fw& fw, const Vc2Run& run);

// The tables both parts read: COSM's pool words and the Matrix's limits
bool vc2LoadCosmAndMatrix(const char* path);

// Whether the check holds type `type`'s converter ported: its own list, the types it draws parameters for
bool vc2CosmCheckPorted(int type);

// A converter's input drawn: noise, then the switch, the type (mostly one the port has), the curves, sensitivities,
// key, velocity and the type's parameters by their ranges; M at `matrixAt`
void vc2DrawCosmInput(Vc2CosmInput& in, std::mt19937& rng, uint32_t matrixAt);
