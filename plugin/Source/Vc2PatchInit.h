#pragma once
// The VC-2's PATCH Init by Patch Category (v473; HARDWARE_FIRMWARE.md, "PATCH Init"): rom.4's own initialize, run in the
// interpreter on the image a .p02's read runs on (vc2PatchRead; the user's rule for load-time firmware).  rom.4's PATCH
// Init window offers the 12 Categories (its buttons put the one chosen at 0x89abb75c) and its EXECUTE (0x89493f40) runs
// 0x89326c60 on part 0's patch with it, which is 0x893291c0(store, no name, category): the store cleared, every ID
// written by rom.4's setters, Patch Category set and a switch of each Category's own settings, the name "INIT PATCH".
// Its check is xt_vc2patch's init part (Vc2PatchInitCheck.cpp).  The V-Synth's is its own (XtFwPatchInit.h), as each
// plugin will have its own after step 6.  Not for the audio thread.
#include "Vc2PatchFile.h"
#include <string>
#include <vector>

constexpr int kVc2PatchCategories = 12;   // [FW] Patch Category (0x12c) 0…11, its descriptor's range

// The initialized patch of Category `category` (0…11): its store and name.  False (and why) for a Category past the
// range, before rom.4 is kept (vc2LoadPatchFiles), or if the routine fails
bool vc2PatchInit(int category, Vc2PatchFile& out, std::string* why = nullptr);

// The Categories' names as rom.4 shows a patch's (its table by Patch Category, 0x8994a638, each two lines, said here
// with a space between), read from rom.4 at each call; none before rom.4 is kept
std::vector<std::string> vc2PatchCategoryNames();
// A label of two lines at `at` in rom.4's memory, one string with a newline between them, said with a space for the
// newline (the names' read, and xt_vc2patch's of the label rom.4 draws)
class Sh4;
std::string vc2CategoryLabelAt(Sh4& cpu, uint32_t at);
