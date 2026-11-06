#pragma once
// A patch's values as XtSynth's controls: patchOf (XtToneCheck.cpp) the other way round, for one zone, and its zones.  No JUCE.
// What a patch has that the Controls don't is listed by xtPatchMissing.  HARDWARE_FIRMWARE.md, "Applying a patch".
#include <cstdint>
#include <string>
#include <vector>
#include "XtSynth.h"

// `base` keeps what isn't a patch's: the system's assigns, Output Mode, Out Gain and Clock Source, the plugin's Pitch Shift, and the wave's own:
// each OSC's Original Key, Original Tempo and loop.  A COSM's type's parameters are read; the other types' go back to the XT's defaults, as a
// patch holds only its own type's.  A COSM of type 0 (Through) is off, which is the same sound.
XtSynth::Controls xtPatchControls(const std::vector<uint8_t>& store, int zone, const XtSynth::Controls& base);

// The upper keys and zones 2…16's controls, as XtSynth::Zones keeps a patch's zones (the user's decision, 2026-11-05); their waves are
// the caller's to find.  `base` as xtPatchControls'.  The zones past xtPatchZones' are read too: no key reaches them.
void xtPatchZoneSet(const std::vector<uint8_t>& store, const XtSynth::Controls& base, XtSynth::Zones& z);

// The Wave Number an OSC plays (IDs 0x2be, 0x2f2), 0 for none.
int xtPatchWaveNumber(const std::vector<uint8_t>& store, int zone, int osc);

// A wave's own settings on its OSC's controls, as the firmware's voice takes them from the wave: Original Key and Tempo, the loop as
// fractions of it, and a Loop forced on where the wave's record says the switch doesn't decide.
struct XtWaveFile;
void xtWaveOntoControls(const XtWaveFile& wave, XtSynth::OscControls& o);
// And on its encoded form: the fine tuning and backward loop, which aren't controls.
void xtWaveOntoEncoded(const XtWaveFile& wave, XtEncoded& e);

// What this zone of the patch asks for that the plugin doesn't do yet, one short phrase each, for the status line.
std::vector<std::string> xtPatchMissing(const std::vector<uint8_t>& store, int zone);

// The firmware's IDs an effect holds in this patch, with their values: the blocks' own and their types' parameters, as XtFxControl::set takes them.
struct XtPatchFx { int id, value; };
std::vector<XtPatchFx> xtPatchEffects(const std::vector<uint8_t>& store);
