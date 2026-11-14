#pragma once
// rom.4's start-up load of the VC-2's samples in the interpreter (research/VC2_VOCAL_DESIGNER.md, "The sample bank,
// loaded"), for the runs that follow it (vc2bank.cpp, vc2wavein.cpp).  The index (`wlflib.py get VC2`) and the bank
// (`wlflib.py bank VC2`, "VC-2 Preset.bin") answered to rom.4's file calls (vc2index.h); the bank's bytes put in a
// region of memory of their own and its base given for the kernel's 0x89336d60, as the preset area is read as memory.
// rom.4's bank object cleared (0x89350af0) and loaded (0x89350970); then the index's parser and the start-up load
// (0x8934d2a0, 0x8934d2d0), which takes the Wave Numbers 1…128 (2999 + n) to the wave library (0x893520a0) and loads each
// VC-2 wave it names (0x8934da70: its maps, each map's samples, 0x8934d8d0 a sample by its index entry, through the
// wave reader 0x893474d0).  The OS's part answered: the preset drive's open (0x893369c0) and close (0x893369f0), the
// display's message (0x8933a890), and the library as the card's link files have it (Wave/001…058: file k names wave k).
#include "vc2index.h"
#include <cstdint>

namespace vc2 {

inline constexpr uint32_t kBank = 0x89e0f034;   // [FW] the bank object, static: +0, +4, then 60-byte entries from +8
inline constexpr uint32_t kBankMapped = 0x8E000000, kBankRoom = 0x01000000;   // the bank's bytes, past the RAM
inline constexpr uint32_t kReadWave = 0x893474d0;   // [FW] the wave reader (path, id, 0)

// A stream in the bank: where it is in memory, and its size
struct Stream {
    uint32_t place = 0, size = 0;
};

struct BankFiles {
    FileImage index, preset;
};
// What the start-up load did: the parser's and the load's results, and how many times it opened the drive
struct StartUp {
    int32_t parsed = 0, loaded = 0;
    long driveOpens = 0;
};

// The two files read, the bank's bytes put at kBankMapped, the kernel answered and the file calls hooked; false, said,
// when a file can't be read or the bank is past its room
bool openBankFiles(Machine& m, BankFiles& f, const char* indexPath, const char* bankPath);
// rom.4's bank object cleared and loaded; false, said, when the loader doesn't give 1
bool loadBank(Machine& m);
// The start-up load's OS calls answered (the drive, the display, the library), its drive's opens counted in `s`
void answerStartUp(Machine& m, StartUp& s);
// The index's parser, then the start-up load within `steps` (the wave reader run takes about 5.7 million a sample);
// false, said, on a fault or when either doesn't give 1
bool runStartUpLoad(Machine& m, const char* who, StartUp& s, uint64_t steps = 200'000'000);
// The path the wave reader is given (r4), at a hook on its first instruction
std::string readerPath(Machine& m, const Sh4& c);
// A sample's stream by rom.4's find (0x89350a50(bank, path + 7, &size), as the reader looks it up: past "D:\VPW\");
// nothing when it isn't found
std::optional<Stream> streamOf(Machine& m, const std::string& path);

} // namespace vc2
