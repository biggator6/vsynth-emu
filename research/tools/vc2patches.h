#pragma once
// The patches run's store helpers (vc2patches.cpp), shared with the runs that read a patch the firmware's way: rom.4's
// default patch made a store, an ID's descriptor and name from rom.4's tables, and the reader rom.4's getter jumps to.
#include "vc2runs.h"
#include <string>
#include <vector>

namespace vc2 {

inline constexpr uint32_t kPatchStore = kScratch + 0x4000;   // where loadStore leaves the store
inline constexpr uint32_t kReadValue = 0x8932b050;           // [FW] (a section's place in the store, id) → the value
inline constexpr uint32_t kToneBase = 0xfe, kToneBlockBytes = 0x80;   // [FW] the tone section: 8 blocks from 0xfe

// [FW] an ID's descriptor, 12 bytes from 0x8950f0b0: u16 byte offset, s8 width, u8 bit, u16 centre, default, min, max
struct Descriptor {
    int offset, width, bit, centre, def, min, max;
};

Descriptor descriptorOf(Machine& m, uint32_t id);
// An ID's name from rom.4's table (pointers to strings with a leading space)
std::string paramName(Machine& m, uint32_t id);
// A .p02's PATa data (its header checked: 0x1e, VC20 at +2, PATaVC_2 at +0x10, the data's offset and length big-endian at
// +0x18, +0x1c), or empty when it isn't a .p02
std::vector<uint8_t> p02Data(const char* path);
// A .p02's data (empty for rom.4's defaults) into rom.4's store at kPatchStore, as its own routines take one; false on a
// fault
bool loadStore(Machine& m, const std::vector<uint8_t>& data);
// Values over the files against a range and a default: lo, hi, how many differ, how many are off the default and outside
// the range
struct ValueSpread {
    int lo, hi;
    long distinct, off, outside;
};
ValueSpread spreadOf(std::vector<int> v, int def, int min, int max);
// An ID's value into the store by its descriptor (big-endian bits from its offset in the section at `section`, the value
// plus its centre), then read back by rom.4's reader (kReadValue); false on a fault or when the reader disagrees, said
bool setStoreValue(Machine& m, uint32_t section, uint32_t id, int value);

} // namespace vc2
