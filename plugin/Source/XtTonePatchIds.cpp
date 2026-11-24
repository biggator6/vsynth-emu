// xt_tone's patch by ID from XtSynth's controls (XtToneCases.h): what the firmware's getter answers.  Moved from
// XtToneCases.cpp as it stood (v195), which keeps the cases' draws, when the zones' splits came in with commonParams.
// From v602 the IDs are the plugin's own (XtPatchIds.cpp, where a patch's save writes them): xt_tone adds its Wave
// Numbers, which a save keeps from the store.
#ifdef XT_TONE_MAIN
#include "XtToneCases.h"
#include "XtToneFirmware.h"
#include "XtPatchIds.h"

static_assert(kCosm2 == kXtCosm2Ids, "xt_tone's COSM 2 IDs are the plugin's");

// And the Multi Step Modulator's, IDs 0x48d…0x4ee (v193)
std::unordered_map<int, int> withMsmIds(std::unordered_map<int, int> p, const XtMsmSettings& s) {
    return xtWithMsmIds(std::move(p), s);
}

std::unordered_map<int, int> patchOf(const Ctl& c) {
    std::unordered_map<int, int> p = xtPatchIds(c);
    for (int n = 0; n < 2; ++n) p[0x2a4 + 0x34 * n + 0x1a] = 1 + n;   // each OSC's Wave Number: the case's own waves
    return p;
}
#endif
