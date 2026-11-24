#pragma once
// A patch by ID from XtSynth's controls, for one zone: xtPatchControls (XtPatch.h) the other way round, what the
// firmware's getter answers for them.  Moved as it stood out of xt_tone's check (XtTonePatchIds.cpp's patchOf, v195) in
// v602, where a patch's save writes them (PluginPatchSave.cpp); xt_tone holds the two together as before.  An OSC's Wave
// Number isn't a control: a save keeps the store's, xt_tone's patchOf adds its own.  No JUCE.
#include <unordered_map>
#include "XtMsm.h"
#include "XtSynth.h"

// COSM 2's IDs here: COSM 1's plus this (xt_tone's kCosm2)
constexpr int kXtCosm2Ids = 0x10000;

std::unordered_map<int, int> xtPatchIds(const XtSynth::Controls& c);
// And the Multi Step Modulator's, IDs 0x48d…0x4ee (v193), beside them
std::unordered_map<int, int> xtWithMsmIds(std::unordered_map<int, int> p, const XtMsmSettings& s);
