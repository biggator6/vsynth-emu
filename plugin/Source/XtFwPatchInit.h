#pragma once
// The V-Synth's PATCH Init (v473; HARDWARE_FIRMWARE.md, "PATCH Init"): rom.2's own initialize, run in the interpreter
// as a patch's read is (xtFwPatchStore; the user's rule for load-time firmware).  The PATCH Init window's EXECUTE
// (0x893c5a10) runs 0x8947fdb0 on part 0's patch, which is 0x89482540(store, no name): the store cleared, every ID's
// value written by the firmware's setters, and the name "INIT PATCH".  Its check is xt_files' init part
// (XtPatchInitCheck.cpp).  Needs xtFwOpen.  Not for the audio thread.
#include "XtFiles.h"
#include <string>

// The initialized patch: its store (kXtPatchBytes, as the firmware holds the patch) and its name.  False (and why) with
// no image, or if the routine fails
bool xtFwPatchInit(XtPatchFile& out, std::string* why = nullptr);
