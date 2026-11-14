#pragma once
// An OSC's per-note input as rom.4 makes it, for the runs that drive the OSC converters (vc2vocosc.cpp,
// vc2splitbytes.cpp): rom.4's default patch made part 0's store, the OSC input builder's record, and the input the
// dispatch and the converter take.
#include "vc2patches.h"
#include <cstdint>
#include <vector>

namespace vc2 {

inline constexpr uint32_t kBuilder = 0x893a04e0;     // [FW] (out, part, zone, osc) → nonzero when it built the record
// [FW] (input, block), as 0x8939fe08 calls them: the dispatch by Type (the block's kind; VOCODER 0x8939ced0's kind 3
// made 5), then the converter (the vibrato's fields, then a converter by kind: VOCODER's 0x8939db00)
inline constexpr uint32_t kDispatch = 0x8939fb40, kConverter = 0x8939fbd0;
inline constexpr uint32_t kPartTable = 0x8950d8e0;   // [FW] a pointer per part to its entry
inline constexpr uint32_t kSwitchId = 0x2e4, kTypeId = 0x2e5, kOscIdStride = 52;   // [FW] OSC 1's; OSC 2's on
inline constexpr uint32_t kRecordBytes = 0x80, kOutHeader = 0x20, kOscStride = 0x80, kNoteHeader = 8;
inline constexpr uint32_t kNotePointer = 0x100;
inline constexpr uint32_t kInputBytes = 0x200, kBlockBytes = 0x1000, kNoteBytes = 0x400;
inline constexpr uint32_t kOscs = 2;

using Bytes = std::vector<uint8_t>;

// Where one note's input, block and note object lie
struct OscNote {
    uint32_t input, block, note;
};

Bytes bytesAt(Machine& m, uint32_t at, uint32_t n);
void zeroBytes(Machine& m, uint32_t at, uint32_t n);
// rom.4's default patch made a store (vc2patches' loadStore), and part 0's entry in the part table (+0 1 when the part
// has a store, +8 the store) pointed at it; false on a fault
bool useDefaultPatch(Machine& m);
// An ID's value into zone 0's tone block by its descriptor, read back by rom.4's reader; false when the reader disagrees
bool setToneValue(Machine& m, uint32_t id, int value);
// Both OSCs on and of one Type; false on a fault
bool setOscType(Machine& m, uint32_t type);
// The OSC input builder's record for one OSC (0x893a04e0(out, part 0, zone 0, osc), a 128-byte record at out + 0x20 +
// osc·0x80); false on a fault or a builder that built nothing
bool buildRecord(Machine& m, uint32_t out, uint32_t osc, Bytes& record);
// The input as 0x8939fe08 makes it, the input, block and note zeroed first: an 8-byte note header (+2 the OSC, +3 key,
// +4 velocity), the record at +8, +0x100 pointing at the note object
void writeOscNote(Machine& m, const OscNote& at, uint32_t osc, uint8_t key, uint8_t velocity, const Bytes& record);

} // namespace vc2
