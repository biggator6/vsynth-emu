#pragma once
// The VC-2's WLF08 wave index read by rom.4's own parser (research/VC2_VOCAL_DESIGNER.md, "A VC-2 wave's split, chosen"),
// for the runs that read a VC-2 wave's splits (vc2split.cpp, vc2splitbytes.cpp).  The index is `wlflib.py get VC2 <out>`'s
// file; the parser's file calls are answered from its bytes.
#include "vc2runs.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vc2 {

inline constexpr uint32_t kWaveSplit = 0x89394b30, kMapSplit = 0x89394c20;   // [FW] (number, value, out)
inline constexpr uint32_t kSampleInfo = 0x89394d10;                          // [FW] (sample, out)
inline constexpr uint32_t kWaveCount = 0x8993448c, kMapCount = 0x8993448e, kSampleCount = 0x89934490;   // [FW] u16s
inline constexpr int kSplitValues = 128;   // a velocity's or a key's

// What the index's own readers give, numbers from 1 (entry 0 unused): each wave's map and one-split byte (out+6, out+0:
// 1 when its first split covers 127) at every velocity, each map's sample and one-split byte at every key, and each
// sample's kind byte (0x89394d10's out+0)
struct IndexChoices {
    std::vector<std::array<uint16_t, kSplitValues>> waveMap, mapSample;
    std::vector<std::array<uint8_t, kSplitValues>> waveOne, mapOne;
    std::vector<uint8_t> kind;
};

inline constexpr const char* kIndexPath = "D:\\VC-2 Wave.bin";   // [FW] the parser's

// A file the OS's file calls are answered from: its bytes, where the next read is, and the address its bytes are at in
// the machine's memory when it is read as memory (0 when not)
struct FileImage {
    std::vector<uint8_t> bytes;
    size_t at = 0;
    uint32_t mapped = 0;
};
struct HookedFile {
    std::string path;
    FileImage* file;   // the caller's, which must outlive the machine's runs
};

// A NUL-ended string in the machine's memory, at most `most` characters
std::string cString(Machine& m, uint32_t at, size_t most);
// The OS's file calls answered from the files: open by path (0x89336c60; the handle a file's place in the list from 1,
// -1 for any other path), read 0x89336cf0, seek 0x89336db0, the file's memory 0x89336d60 (its `mapped`), close
// 0x89336ca0
void hookFiles(Machine& m, std::vector<HookedFile> files);
// The parser (0x8934d12a) on the hooked index: what it returned, or nothing, with why said as `<who>: …`, on a fault
std::optional<int32_t> runParser(Machine& m, const char* who);
// The kernel answered, the file calls hooked to the index alone, and the parser run: what it returned, or nothing, with
// why said, when the file is empty or the parser faults
std::optional<int32_t> parseIndex(Machine& m, FileImage& f, const char* who);
// The readers over the parsed index, `out` their 16-byte answer; nothing on a fault, a reader that finds nothing, or
// one that leaves out+0 as it was (0xff written before each call)
std::optional<IndexChoices> indexChoices(Machine& m, uint32_t out);

} // namespace vc2
