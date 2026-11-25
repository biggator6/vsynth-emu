#pragma once
// A VC-2 patch saved as rom.4 saves one (v505; HARDWARE_FIRMWARE.md, "A VC-2 patch saved"), the mirror of its read
// (Vc2PatchFile.h, v450): the store turned into the file's layout by rom.4's own routine (0x89324f20, the conversion
// 0x89324b20's mirror) and the file written by its .p02 writer (0x893411b0, rom.2's 0x8948fc30 at one distance), its
// open, writes and close answered by hooks that keep the bytes, all run in the interpreter, as v602 saves a .p00 by
// rom.2's (XtFwPatchFile.h).  rom.4's own save of patch n (0x893270a0) is those two, the file named "NNN name.p02" in
// the project's Patch folder (0x89341380).  Not for the audio thread.
#include "XtFwPatchFile.h"
#include <cstdint>
#include <string>
#include <vector>

// The store (kVc2StoreBytes, Vc2PatchStore.h) as a .p02's bytes, on the rom.4 image at `rom4`: false (and why) for a
// store not a patch's size, an image that can't be read, a routine that faults, or the writer giving the file up
bool vc2PatchWrite(const std::string& rom4, const std::vector<uint8_t>& store, std::vector<uint8_t>& file,
                   std::string* why = nullptr);

// The values into `store`, in order, each by rom.4's setter (v506), as xtFwPatchSet sets a .p00's by rom.2's: 0x89328680
// for the first bank (the centre added, held to the descriptor's min and max unless they're equal, compared unsigned),
// 0x89328bf0 for a typed one (from kVc2FirstTyped: held to its type's range, then as its slot's), on rom.4's
// descriptors (0x8950f0b0, 12 bytes an ID; 0x895131a8, 8 bytes a typed one), each ID's place by vc2Store(); a typed
// ID's `base` its block's first slot, or 0 for vc2StoreSlotBase's.  Needs rom.4's tables (vc2LoadParams).  False (and
// why) for a store not a patch's size, an ID outside a patch, or a setter that fails; `store` is then as it was
bool vc2PatchSet(const std::string& rom4, std::vector<uint8_t>& store, const std::vector<XtPatchSet>& values,
                 std::string* why = nullptr);
