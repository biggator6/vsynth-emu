#pragma once
// A wave loaded as the XT loads one (v488): the store made by rom.2's own heap, as its init makes it, and a file put
// in it by the XT's one .wav loader (0x894939d0), which both a project's load and a plain file's import call: an XT
// file's blocks as they are, a plain file's made as the import makes them (LITE, its default vpsc and vplp).  Its file
// calls answered from the file's bytes.  HARDWARE_FIRMWARE.md {#wave-import-read}, {#wave-loader}.
#include "Sh4Interp.h"
#include <cstdint>
#include <string>
#include <vector>

namespace xtfw {

// The file the loader reads, and where it is in it; it must outlive the machine it is given to (its calls stay hooked)
struct WaveIn {
    const std::vector<uint8_t>& bytes;
    std::string path;   // the path the loader is given: its name the wave's (less an "NNN " prefix, 12 characters)
    uint32_t at = 0;
};

// The store made on `c` and `in` loaded into it as wave `key` (rom.2's: the wave's number + 3000); false, and why (the
// loader's answer), if it refuses
bool loadIntoStore(Sh4& c, WaveIn& in, uint32_t key, std::string* why);

} // namespace xtfw
