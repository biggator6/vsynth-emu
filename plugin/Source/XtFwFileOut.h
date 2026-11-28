#pragma once
// A firmware writer run in the interpreter, the OS's file calls it makes answered by hooks (v482, out of v602's
// XtFwPatchFile.cpp): the open gives a handle, each write is kept whole, the close succeeds, and the remove a writer
// calls when a write comes up short is noted.  The .p00 writer's (XtFwPatchFile.cpp) and the wave writer's
// (XtFwWaveFile.cpp): rom.2's own open, write, close and remove, the same four for both.
#include "Sh4Interp.h"
#include <cstdint>
#include <string>
#include <vector>

namespace xtfw {

// A writer and what it is given: its address, called with (path, arg), the path put at `pathAt` first
struct Writer {
    uint32_t at = 0, pathAt = 0;
    const char* path = "";
    uint32_t arg = 0;
};

// The writer run on `c`: the bytes it wrote, in order.  False, and why, if the call faults, or the writer removed its
// file or didn't answer 1 (all of it written).
bool runWriter(Sh4& c, const Writer& w, std::vector<uint8_t>& file, std::string* why);

} // namespace xtfw
