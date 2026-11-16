#pragma once
// What xt_vc2legacy's two parts share (v211): the analysis's (Vc2LegacyCheck.cpp, v210) and the object's
// (Vc2LegacySynthCheck.cpp).  XT_VC2LEGACY_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2CheckWords.h"
#include "Vc2Legacy.h"
#include <bit>
#include <random>
#include <string>

inline constexpr uint32_t kVc2LegacyBlockAt = 0x8C011000;   // the mic's block, in Vc2Fw's RAM
inline constexpr int kVc2LegacyBlocks = 400, kVc2LegacyFadeEvery = 8;   // a case's blocks; every eighth case fades

bool vc2LegacyFirst();   // XT_VC2LEGACY_FIRST: say where the first case to differ first does
// The object's cases (Vc2LegacySynthCheck.cpp) by its mono synthesis (channels 1) or its stereo (2, v212), from the
// analysis's state as the last cases left it, which `carried` takes back at the end; the cases that differ
long vc2LegacySynthesisCases(Vc2Fw& fw, Vc2LegacyAnalysis& carried, const Vc2Run& run, int channels);
