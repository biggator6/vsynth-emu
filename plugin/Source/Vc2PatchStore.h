#pragma once
// rom.4's (the VC-2's) patch store (v450): where each of the IDs a patch holds lies in the store that rom.4's conversion
// writes from a .p02's data (0x89324b20), by rom.4's own pointing routines (0x893207b0…0x89320da0), read by XtFiles.h's
// store reads on rom.4's descriptors (Vc2Params): HARDWARE_FIRMWARE.md, "rom.4's patch store and a .p02".  A store comes
// from vc2PatchRead (Vc2PatchFile.h).  An ID reads 0 until rom.4's tables are in.  Its check is xt_vc2patch's
// (Vc2PatchStoreCheck.cpp).
#include "XtFiles.h"
#include <vector>

constexpr int kVc2StoreBytes = 0x13a4;   // [FW] the store's, to the arpeggio pattern's last line, and a file's data at most

// rom.4's store: its sections (a zone's, a tone block's or an arpeggio pattern's line `zoneStride` apart), its COSMs
// twice over and its descriptors, for xtStoreSection, xtStoreRaw and xtStoreValue
const XtStoreLayout& vc2Store();
// The first-bank ID a typed ID's block counts its slots from (MFX's, Chorus's, Reverb's or a COSM's); -1 below
// kVc2FirstTyped or past the table
int vc2StoreSlotBase(int id);
// A typed ID (from kVc2FirstTyped), which its block's Type gives a slot's meaning, as rom.4's typed reader (0x8932b460)
// reads it from its block's slots: `zone` and `cosm` a COSM's
int vc2StoreTyped(const std::vector<uint8_t>& store, int id, int zone = 0, int cosm = 0);
