#pragma once
// The VC-2's PATCH Init by Patch Category on the processor (v473; HARDWARE_FIRMWARE.md, "PATCH Init: the V-Synth's,
// and the VC-2's by Patch Category"): rom.4's own initialize of the Category chosen, run in the interpreter (Vc2PatchInit.h), then applied
// as a .p02 is (VSynthEmuProcessor::applyVc2Patch): its values onto the VC-2's parameters, its store and name kept for
// the session and the part.  The VC-2's system parameters and every V-Synth one stay.  The V-Synth's init is its own
// (PluginPatchInit.h), as each plugin will have its own after step 6.  Message thread.
#include <string>

class VSynthEmuProcessor;

// False (and why) for a Category past 0…11, before rom.4 is kept, or if rom.4's routine fails; nothing moved then
bool initVc2Patch(VSynthEmuProcessor& p, int category, std::string* why = nullptr);
