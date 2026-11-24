#pragma once
// xt_vc2synth's shared parts (v343): a region of the part against rom.4's memory byte for byte, every region in turn,
// and the further parts: C's reads of the part (Vc2SynthReadsCheck.cpp).  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2CheckRig.h"
#include "Vc2SynthMemory.h"
#include <random>
#include <string>

// The first byte of the region that differs from rom.4's at its address, said; none
std::string vc2SynthBytesDifference(Vc2Fw& fw, const Vc2SynthRegion& r);
// The first region of the part that differs, said; none
std::string vc2SynthRegionsDifference(Vc2Fw& fw, Vc2Synth& s);
// v345: the regions, then the vocoder's engines, stages, engine word and hook word (the start's compare)
std::string vc2SynthDifference(Vc2Fw& fw, Vc2Synth& s);

// The load part (v345, Vc2SynthLoadCheck.cpp): the notes' task's start, then `run.cases` cases of a drawn store (or
// none) and 20 events of the part's readers and messages against rom.4's; how many differ
long vc2SynthLoadPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run);

// The reads part (v343): the regions apart; every read of C's over its arguments reaching only the part's regions; then
// `run.cases` cases, each the part's memory read by address against rom.4's after noise in drawn regions, the Matrix's
// ten readers, the OSC's getters and a sum on drawn Ms against rom.4's, and the children's placements; how many differ
long vc2SynthReadsPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run);
#endif
