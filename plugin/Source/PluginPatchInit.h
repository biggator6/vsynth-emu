#pragma once
// The V-Synth's PATCH Init on the processor (v473; HARDWARE_FIRMWARE.md, "PATCH Init"): rom.2's own initialize run in
// the interpreter (XtFwPatchInit.h), then applied as a loaded patch is (VSynthEmuProcessor::applyPatch): zone 1 onto the
// parameters, its effects and arpeggio, the zones it has and none else, its name "INIT PATCH".  As on the XT, only the
// patch being played changes: the system's settings, the Effects switch and the patch browsed stay.  The VC-2 mode has
// none (v474: the user's "vc-2 mode doesn't need an init").  Message thread.
#include <string>

class VSynthEmuProcessor;

// False (and why) with no firmware image, or if the firmware's routine fails; nothing moved then
bool initPatch(VSynthEmuProcessor& p, std::string* why = nullptr);
