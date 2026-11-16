#pragma once
// xt_vc2carrier's parts (v222): the carrier's (Vc2CarrierCheck.cpp) and the VOCODER OSC's converter's and kind 5's
// update's (Vc2VocoderOscCheck.cpp), on one interpreter.  XT_VC2CARRIER_MAIN only.
#include "Vc2CheckRig.h"

constexpr uint32_t kVc2OsClock = 0x893018d0;   // [FW] the OS's millisecond word, which the check's hook answers

// The second part's cases: what differs, counted (0 when nothing does); `clock` is the word the clock's hook answers
long vc2VocoderOscCases(Vc2Fw& fw, const Vc2Run& run, int32_t& clock);
