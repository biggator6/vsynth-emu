#pragma once
// The VC-2's wave sets (v399): rom.4's three tables a set's index is looked up in, and the wave's words by a set's
// index, the key and the velocity, 0x8939cba0, which PCM's start (0x8939e120) calls when the store (Vc2WaveStore, v398)
// holds no record of its Wave Number loaded.  Each set is a count, a table and a buffer its reads fill, word for word at
// their addresses.  The velocity's set (0x89394b30) and the key's (0x89394c20), one routine over a row: the table's
// entry a stream of the wave memory, its first 0x210 or 0x310 bytes read into the buffer (0x89327b40), then 128 entries
// of 4 or 6 bytes from +0x10, the first whose upper bound the velocity or key isn't above.  The alias's set
// (0x89394d10, its stream 0x89394af0 its table's own entry): an entry's type and the number of the record it names, its
// own or its link's when the linked entry's type is 1.  0x8939cba0 then fills the wave's words of the store's record of
// that number as 0x8939cd30 does, its +4…+7 from the sets.  A count past its table's 500 is held to 500, where rom.4
// would read on into the next table (the sets' loader, the index's parse, keeps them within: Vc2WaveIndex, v500, which
// fills the counts, the tables and the data).  Bit for bit: xt_vc2sets' eighth part.  HARDWARE_FIRMWARE.md, "The
// VC-2's wave sets".  The notes' task.
#include "Vc2WaveStore.h"
#include <array>
#include <cstdint>
#include <vector>

// [FW] the counts at 0x8993448c (.data, 0 after C startup): the velocity's set's, the key's and the alias's
struct Vc2WaveSetCounts {
    uint16_t velocity = 0;
    uint16_t key = 0;
    uint16_t alias = 0;
};
static_assert(sizeof(Vc2WaveSetCounts) == 6, "rom.4's layout, the sets' counts");

// [FW] the tables at 0x89973084 (.bss), 500 entries each: the velocity's and the key's two words an entry, a stream of
// the wave memory (its start, then its length); an alias's four bytes, its type, a byte and its link (a halfword)
inline constexpr uint32_t kVc2WaveSetEntries = 500;
struct Vc2WaveSetTables {
    std::array<uint32_t, 2 * kVc2WaveSetEntries> velocity {};
    std::array<uint32_t, 2 * kVc2WaveSetEntries> key {};
    std::array<std::array<uint8_t, 4>, kVc2WaveSetEntries> alias {};
};
static_assert(sizeof(Vc2WaveSetTables) == 0x2710, "rom.4's layout, the sets' tables");

// [FW] the buffers at 0x89989754 (.bss): what each set's last read brought
struct Vc2WaveSetBuffers {
    std::array<uint8_t, 0x210> velocity {};
    std::array<uint8_t, 0x310> key {};
    std::array<uint8_t, 4> alias {};
};
static_assert(sizeof(Vc2WaveSetBuffers) == 0x524, "rom.4's layout, the sets' buffers");

// [FW] the velocity's and the key's data (.bss, v500): a slot an entry, 0x210 and 0x310 bytes, which the index's parse
// (Vc2WaveIndex) fills and points the entry's stream at
inline constexpr uint32_t kVc2VelocitySlot = 0x210, kVc2KeySlot = 0x310;
struct Vc2WaveSetData {
    std::vector<uint8_t> velocity = std::vector<uint8_t>(kVc2WaveSetEntries * kVc2VelocitySlot);
    std::vector<uint8_t> key = std::vector<uint8_t>(kVc2WaveSetEntries * kVc2KeySlot);
};

class Vc2WaveSets {
public:
    static constexpr uint32_t kCountsAt = 0x8993448c, kTablesAt = 0x89973084, kBuffersAt = 0x89989754;
    static constexpr uint32_t kVelocityDataAt = 0x89d65e38, kKeyDataAt = 0x89da6578;
    Vc2WaveSetCounts counts {};
    Vc2WaveSetTables tables {};
    Vc2WaveSetBuffers buffers {};
    Vc2WaveSetData data {};
    // The data's byte at rom.4's address `at`, for the wave memory that reads the streams; none outside the data
    const uint8_t* dataAt(uint32_t at) const;
    // 0x8939cba0: the wave's words (the OSC input's +0xa0…, `wave`) by a set's index, the key and the velocity, from
    // the store's record the sets name; whether found
    bool waveWords(uint16_t index, uint8_t* wave, uint8_t key, uint8_t velocity, const Vc2WaveStore& store,
                   const Vc2WaveMemory& m);
};
