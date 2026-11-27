#pragma once
// A VC-2 patch file, .p02 (v450), into rom.4's store (Vc2PatchStore.h): read by rom.4's own reader (0x893419a0) in the
// interpreter, its four file calls answered from the file's bytes, over rom.4's default patch (0x89325300), then turned
// into the store by rom.4's conversion (0x89324b20), as XtFiles reads a .p00 by rom.2's.  HARDWARE_FIRMWARE.md, "rom.4's
// patch store and a .p02".  Its check is xt_vc2patch's (Vc2PatchStoreCheck.cpp).  Not for the audio thread.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct Vc2PatchFile {
    std::string name;             // its 12 characters, trailing spaces gone
    std::vector<uint8_t> store;   // kVc2StoreBytes, as rom.4 holds a patch
};
// loadVc2's (XtSynthRom.cpp): the image a file's read runs on, kept for the process, once; true when it's kept
[[nodiscard]] bool vc2LoadPatchFiles(const char* path);
bool vc2PatchFilesLoaded();
// The image's path as kept, for rom.4's patch save (Vc2PatchSave.h, v505); empty until vc2PatchFilesLoaded()
const std::string& vc2PatchFilesRom();
// A .p02 into its store: false, and why, for a file rom.4's reader refuses, one under its two 16-byte headers (whose
// short count rom.4's caller would take as read, with nothing laid out), a fault, or before the image is kept
bool vc2PatchRead(const uint8_t* file, size_t size, Vc2PatchFile& out, std::string* why = nullptr);
// A store's name (Patch Name 1-12), trailing spaces gone: the read's, and (v473) the PATCH Init's (Vc2PatchInit.h)
std::string vc2PatchStoreName(const std::vector<uint8_t>& store);
// The store a rom.4 routine left at `at` in `cpu`'s memory, and its name, into `out` (the read's, and v473's init)
class Sh4;
void vc2PatchTake(Sh4& cpu, uint32_t at, Vc2PatchFile& out);
