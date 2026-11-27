#pragma once
// xt_vc2patch's parts (v450): the store's (Vc2PatchStoreCheck.cpp, with the main) and the files' (Vc2PatchFileCheck.cpp).
// XT_VC2PATCH_MAIN builds only.
#include "Vc2CheckRig.h"
#include "Vc2PatchFile.h"
#include <cstdio>
#include <string>
#include <vector>

inline constexpr long kVc2PatchShown = 5;   // differences said under a part's line
// rom.4's memory for the check, in the RAM Vc2Fw::start maps: the stores the pointing routines aim at, the read object,
// the part table's entry, and the file image and store of rom.4's own read of a file's data
inline constexpr uint32_t kVc2StoreA = 0x8C020000, kVc2StoreB = 0x8C030000, kVc2Object = 0x8C040000;
inline constexpr uint32_t kVc2Entry = 0x8C041000, kVc2FileImage = 0x8C050000, kVc2FileStore = 0x8C060000;
inline constexpr uint32_t kVc2Parts = 0x8950d8e0;   // [FW] a pointer a part: +0 1 when it has a store, +8 it, +4 the other
// Part 0's entry in the part table: a store at +8 (A) and +4 (B), the one rom.4's PATCH Init writes (Vc2PatchStoreCheck.cpp)
void vc2PointAtStores(Vc2Fw& fw);

// A part's count of what it compared and what differed, the first few said under its line
struct Vc2PatchTally {
    const char* what;
    long compared = 0, different = 0;
    void differs(const std::string& line) {
        if (different++ < kVc2PatchShown) std::printf("  %s\n", line.c_str());
    }
    long say(const std::string& how) const {
        std::printf("%s: %ld %s, %ld different\n", what, compared, how.c_str(), different);
        return different;
    }
};

// A patch file of the disk image's Internal.vc2/Patch: its name there and its bytes
struct Vc2PatchOnDisk {
    std::string name;
    std::vector<uint8_t> bytes;
};

// The files' part: each file through vc2PatchRead against rom.4's routines on its data straight, and its name against
// the file's; then the files' headers and lengths made otherwise, each against what rom.4's reader takes of it
long vc2PatchFiles(Vc2Fw& fw, const std::vector<Vc2PatchOnDisk>& files);

// rom.4's patch save's parts (v505, Vc2PatchSaveCheck.cpp): the files and `cases` drawn stores written by vc2PatchWrite
// and read back; and every ID set by vc2PatchSet on the first file's store and read back
long vc2PatchSaves(const std::vector<Vc2PatchOnDisk>& files, int cases, int seed);
long vc2PatchSetters(const std::vector<Vc2PatchOnDisk>& files, int seed);

// rom.4's PATCH Init by Patch Category (v473, Vc2PatchInitCheck.cpp): the window's EXECUTE run at each Category on its
// own machine of `rom`, against vc2PatchInit; the other window's, CANCEL, and the default under a .p02.  And the 12
// inits, for the store check's reads (none if one fails)
long vc2PatchInits(const std::string& rom, const std::vector<Vc2PatchOnDisk>& files);
std::vector<Vc2PatchFile> vc2PatchInitStores();
