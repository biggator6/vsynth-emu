#pragma once
// The VC-2's wave store (v398): rom.4's store at 0x8997c52c, which keeps a record for each wave it has loaded, and the
// lookups PCM's start (0x8939e120, Vc2SetOscKinds) makes in it.  The store's own words are its key and 128 lists; the
// records, their nodes and the chunks' blocks they point at lie in the wave memory, read by rom.4's address through
// `Vc2WaveMemory`.  A record (0x8949e5c0's of rom.2, the twin of 0x89351840, gives its wave by chunk number, as
// xt_tone's hook of it does): 2 the vpsc's block, 3 the vpno's, 4 the vplp's, 5 the vpdt's, 10 the cached loop events',
// a block's data 28 bytes in.  Ported: the find 0x89353130 under the store's key, whether a record is loaded
// 0x89351840, a set's index 0x893520a0, and the wave's words by a Wave Number 0x8939cd30 with the getters it calls
// (0x89351ad0's chunks 2, 3, 4, 5 and 10, 0x89351ff0's data words, 0x89351a10's frames).  The getters each find the
// record again under the same key, which 0x89351840 found, so they read it; their fields rom.4's callers never ask for
// (0x89351ad0's 1, 6…9, 11, 12 and past 12) aren't ported, and neither is the store's set-up or insert (0x893531c0).
// The key: a Wave Number less 1 plus 3000, a halfword, or the store's own while not negative, so never negative.  Bit
// for bit: xt_vc2sets' eighth part.  HARDWARE_FIRMWARE.md, "The VC-2's wave store".  The notes' task.
#include <array>
#include <cstdint>

// The memory a wave's record and blocks lie in, by rom.4's address: a byte, and a halfword and a word little-endian, as
// rom.4's loads and its unaligned readers 0x89300db0 and 0x89300dd0 read them
class Vc2WaveMemory {
public:
    virtual ~Vc2WaveMemory() = default;
    virtual uint8_t byte(uint32_t at) const = 0;
    uint16_t half(uint32_t at) const;
    uint32_t word(uint32_t at) const;
};

// [FW] the store's words at 0x8997c52c: a key every lookup takes in place of its own while it isn't negative, then 128
// lists of records by key mod 128, each a node's address (a record's +0x18) or 0; .bss, so 0 after C startup
struct Vc2WaveStoreWords {
    int32_t key = 0;
    std::array<uint32_t, 128> heads {};
};
static_assert(sizeof(Vc2WaveStoreWords) == 0x204, "rom.4's layout, the wave store");

class Vc2WaveStore {
public:
    static constexpr uint32_t kAt = 0x8997c52c;
    Vc2WaveStoreWords words {};
    // The record of `key` (by the store's key): 0x89353130's node less 24, or 0
    uint32_t recordOf(uint32_t key, const Vc2WaveMemory& m) const;
    // 0x89351840: that record when it holds its wave loaded (no set's block at +0x4c, its halfword +0x52 clear), else 0
    uint32_t loadedRecord(uint32_t key, const Vc2WaveMemory& m) const;
    // 0x893520a0: the record's set's index, its block's first word; −1 for no record, −2 for no set
    int32_t setIndex(uint32_t key, const Vc2WaveMemory& m) const;
};

// 0x8939cd30: the wave's words (the OSC input's +0xa0…+0x5b, `wave`) from the store by a Wave Number (none for 0),
// whether its record is loaded
bool vc2WaveWords(const Vc2WaveStore& store, uint16_t number, uint8_t* wave, const Vc2WaveMemory& m);
