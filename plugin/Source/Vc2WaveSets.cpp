// The VC-2's wave sets: see Vc2WaveSets.h (v399).  The wave's words stored as the store's are (Vc2PartWords), a
// buffer's halfwords read as a converter reads its input (vc2in).
#include "Vc2WaveSets.h"
#include "Vc2InputReads.h"
#include "Vc2PartSettingsWords.h"
#include <algorithm>
#include <span>

using Vc2PartWords::put32;

namespace {
// [FW] a velocity's or key's buffer: 128 entries from +0x10, the first entry's bound from 127 a flag; the header's +0xa
// and +0xb " M" the velocity's mark
constexpr uint32_t kEntriesAt = 0x10, kEntries = 128, kFlagFrom = 127, kMarkAt = 0xa;
constexpr uint8_t kMark[] = { ' ', 'M' };
constexpr uint8_t kLinkedType = 1;   // [FW] an alias's link taken when the linked entry's type is 1
// [FW] the wave's words 0x8939cba0 writes from the sets: found +0 (both sets' flags), the velocity's level +4, the
// key's root offset +5 and level +6, the alias's type +7, the velocity's mark +0x5c
constexpr int kFoundAt = 0, kVelocityLevelAt = 4, kRootOffsetAt = 5, kKeyLevelAt = 6, kTypeAt = 7, kMarkedAt = 0x5c;

// What a velocity's or key's set gives: its flag, the two bytes after the entry's bound and its halfword (the next
// set's number), the velocity's mark
struct Split {
    bool flag = false;
    uint8_t first = 0, second = 0;
    uint16_t next = 0;
    bool marked = false;
};

// [FW] the two sets as rom.4 reads them: the count, the table, the buffer, an entry's bytes and its halfword's place
using SetTable = std::array<uint32_t, 2 * kVc2WaveSetEntries>;
struct SplitRow {
    uint16_t Vc2WaveSetCounts::*count;
    SetTable Vc2WaveSetTables::*table;
    std::span<uint8_t> (*buffer)(Vc2WaveSetBuffers&);
    uint32_t entryBytes, nextAt;
};
std::span<uint8_t> velocityBuffer(Vc2WaveSetBuffers& b) { return b.velocity; }
std::span<uint8_t> keyBuffer(Vc2WaveSetBuffers& b) { return b.key; }
constexpr SplitRow kVelocity { &Vc2WaveSetCounts::velocity, &Vc2WaveSetTables::velocity, velocityBuffer, 4, 2 };
constexpr SplitRow kKey { &Vc2WaveSetCounts::key, &Vc2WaveSetTables::key, keyBuffer, 6, 4 };

// What the alias's set gives: its entry's type, and the number of the store's record to take
struct Alias {
    uint8_t type = 0;
    uint16_t number = 0;
};

// A set's count, held to its table's 500
uint32_t heldCount(uint16_t count) { return std::min<uint32_t>(count, kVc2WaveSetEntries); }

// 0x89327b40 on a stream from `at`, `bytes` long: what's asked or, when less, what's left, copied into `into`; how many
uint32_t streamRead(uint32_t at, uint32_t bytes, std::span<uint8_t> into, const Vc2WaveMemory& m) {
    const uint32_t k = into.size() > bytes ? bytes : static_cast<uint32_t>(into.size());
    for (uint32_t i = 0; i < k; ++i) into[i] = m.byte(at + i);
    return k;
}

// 0x89394b30, 0x89394c20: the set's entry by number (from 1), its buffer read whole, the first of its entries whose
// bound `by` isn't above; whether found
bool splitOf(Vc2WaveSets& s, const SplitRow& row, uint32_t n, uint8_t by, Split& out, const Vc2WaveMemory& m) {
    if (n == 0 || n > heldCount(s.counts.*row.count)) return false;
    const SetTable& table = s.tables.*row.table;
    const std::span<uint8_t> buffer = row.buffer(s.buffers);
    if (streamRead(table[2 * (n - 1)], table[2 * (n - 1) + 1], buffer, m) != buffer.size()) return false;
    out.flag = buffer[kEntriesAt] >= kFlagFrom;
    out.marked = buffer[kMarkAt] == kMark[0] && buffer[kMarkAt + 1] == kMark[1];
    for (uint32_t i = 0; i < kEntries; ++i) {
        const uint8_t* entry = buffer.data() + kEntriesAt + row.entryBytes * i;
        if (by > entry[0]) continue;
        out.first = entry[1];
        out.second = entry[2];
        out.next = static_cast<uint16_t>(vc2in::s16(entry, static_cast<int>(row.nextAt)));
        return true;
    }
    return false;
}

// 0x89394d10 (0x89394af0's stream its table's entry, read whole into the buffer): the entry by number (from 1), and its
// link's number when the linked entry's type is 1, the link read below the number only; whether found.  The link's
// own bound in 0x89394af0 can't fail there: it is below n, which is within the count
bool aliasOf(Vc2WaveSets& s, uint32_t n, Alias& out) {
    if (n == 0 || n > heldCount(s.counts.alias)) return false;
    s.buffers.alias = s.tables.alias[n - 1];
    out.type = s.buffers.alias[0];
    out.number = static_cast<uint16_t>(n);
    const auto link = static_cast<uint16_t>(vc2in::s16(s.buffers.alias.data(), 2));
    if (link == 0 || n <= link) return true;
    s.buffers.alias = s.tables.alias[link - 1];
    if (s.buffers.alias[0] == kLinkedType) out.number = link;
    return true;
}
} // namespace

const uint8_t* Vc2WaveSets::dataAt(uint32_t at) const {
    if (at - kVelocityDataAt < data.velocity.size()) return &data.velocity[at - kVelocityDataAt];
    if (at - kKeyDataAt < data.key.size()) return &data.key[at - kKeyDataAt];
    return nullptr;
}

bool Vc2WaveSets::waveWords(uint16_t index, uint8_t* wave, uint8_t key, uint8_t velocity, const Vc2WaveStore& store,
                            const Vc2WaveMemory& m) {
    Split byVelocity, byKey;
    Alias alias;
    const bool named = splitOf(*this, kVelocity, index, velocity, byVelocity, m) &&
                       splitOf(*this, kKey, byVelocity.next, key, byKey, m) && aliasOf(*this, byKey.next, alias);
    if (!named) return false;
    put32(wave + kMarkedAt, byVelocity.marked ? 1 : 0);
    put32(wave + kFoundAt, byVelocity.flag && byKey.flag ? 1 : 0);
    wave[kVelocityLevelAt] = byVelocity.first;
    wave[kRootOffsetAt] = byKey.second;
    wave[kKeyLevelAt] = byKey.first;
    wave[kTypeAt] = alias.type;
    return vc2WaveLoadedWords(store, alias.number, wave, m, false);
}
