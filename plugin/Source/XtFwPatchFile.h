#pragma once
// A patch saved as the firmware saves one (v602; HARDWARE_FIRMWARE.md, "A patch saved"): values set into the store by its
// own setters, the store turned into a file's layout and the file written by its own routines, all run in the
// interpreter, as a patch's read is (xtFwPatchStore; the user's rule for load-time firmware, the save its mirror).
// Needs xtFwOpen.  Not for the audio thread.
#include "XtFiles.h"
#include <cstdint>
#include <string>
#include <vector>

// One value to set: an ID of either bank (from 0x55d a typed one, `base` the first slot of its block, as xtPatchTyped
// takes it), its zone (0..15) and COSM (0..1) where its section repeats, and the value as the firmware's read gives it,
// less the centre
struct XtPatchSet {
    int id = 0, value = 0, zone = 0, cosm = 0, base = 0;
};

// The values into `store`, in order, each by the firmware's setter: 0x894818b0 for the first bank (the centre added, the
// bits clamped to the descriptor's range unless its min is its max), 0x89481e20 for a typed one (clamped to the type's
// range, then as its slot's).  False (and why) for a store not a patch's size, an ID outside a patch, or a setter that
// fails; `store` is then as it was
bool xtFwPatchSet(std::vector<uint8_t>& store, const std::vector<XtPatchSet>& values, std::string* why = nullptr);

// The store as a .p00 file's bytes: its file layout by 0x8947de90 (0x8947dae0's mirror), the file as the writer
// 0x8948fc30 writes it, its open, writes and close answered by hooks that keep the bytes.  False (and why) if a routine
// fails or the writer gives the file up
bool xtFwPatchFile(const std::vector<uint8_t>& store, std::vector<uint8_t>& file, std::string* why = nullptr);
