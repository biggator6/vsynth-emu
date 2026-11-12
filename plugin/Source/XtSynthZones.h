#pragma once
// A patch's zones, which XtSynth plays: moved from XtSynth.h as they stood (v193); XtSynth::Zones names them.
#include <memory>
#include "XtEncoded.h"
#include "XtSynthControls.h"

constexpr int kXtZones = 16;

// A patch's zones (v154, the user's decision of 2026-11-05: a loaded patch plays all of them, kits too, while the plugin's
// parameters and editor stay on zone 1).  Zone 1 is setControls' and setSample's; zones 2…16 are a patch's data, each its
// controls and its two waves.  Only a zone's own blocks are read from it (the tone block, envelopes, LFOs and COSMs:
// zoneControls); the patch's common and the system's are zone 1's, as on the XT, where they aren't in a zone.  The upper keys
// (IDs 0x124…0x133) pick a note's zone.
struct XtZones {
    int upper[kXtZones] = { 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
    XtControls ctl[kXtZones];                                // [0] unused: zone 1 is setControls'
    std::shared_ptr<const XtEncoded> wave[kXtZones][2];      // the same
    int number[kXtZones][2] = {};                            // their Wave Numbers, as setSample's; 0 a new wave
};
