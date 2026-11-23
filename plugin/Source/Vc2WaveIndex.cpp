// The VC-2's wave index, parsed: see Vc2WaveIndex.h (v500).  Every offset and count is the file's, read little-endian
// as rom.4's loads read the bytes its reads leave.
#include "Vc2WaveIndex.h"
#include "Vc2InputReads.h"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace {
constexpr const char* kIndexPath = "D:\\VC-2 Wave.bin";   // [FW] 0x895161b4
constexpr uint8_t kMagic[] = { 'W', 'L', 'F', '0' };      // [FW] 0x895161c8, its four bytes compared
// [FW] the file's layout: a header of 12 bytes (the magic, then the sections' count at +8), the version strings from 16,
// 16 characters each, and the sections' entries from 0x40, 16 bytes each (12 read: the number, a halfword 1, where)
constexpr uint32_t kHeaderBytes = 12, kSectionsCountAt = 8, kVersionsAt = 16, kVersionChars = 16;
constexpr uint32_t kSectionsAt = 0x40, kSectionStride = 16, kSectionRead = 12, kNumberAt = 0, kOnAt = 2, kPlaceAt = 4;
constexpr int kOn = 1;   // [FW] the entry's second halfword when it is read
// [FW] a section's header: its entries' count (a halfword of a word), where its index is in it, an index entry's size;
// a velocity's or key's index entry: its record's place in the section, then its size
constexpr uint32_t kEntriesAt = 0, kIndexAt = 4, kEntrySizeAt = 8, kRecordAt = 0, kRecordSizeAt = 4, kIndexEntry = 8;
constexpr uint32_t kAliasAt = 32, kAliasBytes = 4;   // [FW] a sample entry's four bytes after its 32-byte path
constexpr uint16_t kSectionStep = 33, kParseDone = 100;   // [FW] the progress after each section, and at the end
constexpr int32_t kFromStart = 0;

uint32_t fileWord(const uint8_t* p, uint32_t at) { return vc2in::u32(p, static_cast<int>(at)); }
uint16_t fileHalf(const uint8_t* p, uint32_t at) { return static_cast<uint16_t>(vc2in::s16(p, static_cast<int>(at))); }
int32_t seekAt(uint32_t at) { return static_cast<int32_t>(at); }   // a seek's offset, as rom.4's register holds it

// A read into the reader's words from `at` on, as far as their end
void readIntoWords(Vc2Files& files, int32_t handle, Vc2WaveIndexWords& words, size_t at, uint32_t n) {
    auto* bytes = reinterpret_cast<uint8_t*>(&words);
    files.read(handle, bytes + at, static_cast<uint32_t>(std::min<size_t>(n, sizeof words - at)));
}

// [FW] a velocity's or key's section as the parse fills its set: the set's count, table, data and slot, where the data
// lies, and its buffer in the reader's words (0x8997af52, 0x8997b162)
using SetTable = std::array<uint32_t, 2 * kVc2WaveSetEntries>;
struct SetRow {
    uint16_t Vc2WaveSetCounts::*count;
    SetTable Vc2WaveSetTables::*table;
    std::vector<uint8_t> Vc2WaveSetData::*data;
    uint32_t dataAt, slot;
    size_t readAt;
};
const SetRow kVelocity { &Vc2WaveSetCounts::velocity, &Vc2WaveSetTables::velocity, &Vc2WaveSetData::velocity,
                         Vc2WaveSets::kVelocityDataAt, kVc2VelocitySlot, offsetof(Vc2WaveIndexWords, velocityRead) };
const SetRow kKey { &Vc2WaveSetCounts::key, &Vc2WaveSetTables::key, &Vc2WaveSetData::key, Vc2WaveSets::kKeyDataAt,
                    kVc2KeySlot, offsetof(Vc2WaveIndexWords, keyRead) };

// 0x89327bc0, 0x89327c50, 0x89327ce0's count: a set's count raised to entry n (from 1); false past the table's 500
bool raiseCount(uint16_t& count, uint32_t n) {
    if (n > kVc2WaveSetEntries) return false;
    if (count < n) count = static_cast<uint16_t>(n);
    return true;
}

// 0x8934ce90, 0x8934cf40: each record by its index entry, read into the set's buffer, then (0x89327bc0, 0x89327c50) its
// slot zeroed and the table's stream made the slot, and (0x89327b80) the record written into the stream, as much as
// the slot holds.  The stream's own bound (n within the count) can't fail: the slot has just raised the count to n
void setSection(const SetRow& row, const Vc2WaveIndexState& state, Vc2Files& files, Vc2WaveIndexWords& words,
                Vc2WaveSets& sets) {
    uint8_t header[kHeaderBytes];
    files.read(state.handle, header, kHeaderBytes);
    const uint16_t entries = fileHalf(header, kEntriesAt);
    const uint32_t indexAt = state.sectionAt + fileWord(header, kIndexAt), entryBytes = fileWord(header, kEntrySizeAt);
    for (uint32_t n = 1; n <= entries; ++n) {
        files.seek(state.handle, seekAt(indexAt + (n - 1) * entryBytes), kFromStart);
        uint8_t entry[kIndexEntry];
        files.read(state.handle, entry, kIndexEntry);
        files.seek(state.handle, seekAt(state.sectionAt + fileWord(entry, kRecordAt)), kFromStart);
        const uint32_t size = fileWord(entry, kRecordSizeAt);
        readIntoWords(files, state.handle, words, row.readAt, size);
        if (!raiseCount(sets.counts.*row.count, n)) return;
        uint8_t* slot = (sets.data.*row.data).data() + row.slot * (n - 1);
        std::fill_n(slot, row.slot, 0);
        (sets.tables.*row.table)[2 * (n - 1)] = row.dataAt + row.slot * (n - 1);
        (sets.tables.*row.table)[2 * (n - 1) + 1] = row.slot;
        std::copy_n(reinterpret_cast<const uint8_t*>(&words) + row.readAt, std::min(size, row.slot), slot);
    }
}

// 0x8934d050: the samples' entries one after another from the section's index, each read into the alias's buffer, its
// four bytes after the path then (0x89327ce0, then a stream on the table's own entry) the alias's entry
void aliasSection(const Vc2WaveIndexState& state, Vc2Files& files, Vc2WaveIndexWords& words, Vc2WaveSets& sets) {
    uint8_t header[kHeaderBytes];
    files.read(state.handle, header, kHeaderBytes);
    files.seek(state.handle, seekAt(state.sectionAt + fileWord(header, kIndexAt)), kFromStart);
    const uint16_t entries = fileHalf(header, kEntriesAt);
    for (uint32_t n = 1; n <= entries; ++n) {
        readIntoWords(files, state.handle, words, offsetof(Vc2WaveIndexWords, aliasRead),
                      fileWord(header, kEntrySizeAt));
        if (!raiseCount(sets.counts.alias, n)) return;
        std::copy_n(words.aliasRead.data() + kAliasAt, kAliasBytes, sets.tables.alias[n - 1].data());
    }
}
} // namespace

int32_t Vc2WaveIndex::parse(Vc2WaveIndexState& state, Vc2Files& files, Vc2WaveSets& sets) {
    progress.parse = 0;
    state.handle = files.open(kIndexPath);
    if (state.handle < 0) return kVc2IndexMissing;
    uint8_t header[kHeaderBytes];
    files.read(state.handle, header, kHeaderBytes);
    if (std::memcmp(header, kMagic, sizeof kMagic) != 0) {
        files.close(state.handle);
        return kVc2IndexNotWlf;
    }
    files.seek(state.handle, kVersionsAt, kFromStart);
    words.waveVersion.fill(0);
    files.read(state.handle, words.waveVersion.data(), kVersionChars);
    words.presetVersion.fill(0);
    files.read(state.handle, words.presetVersion.data(), kVersionChars);
    for (uint32_t i = 0; i < fileWord(header, kSectionsCountAt); ++i) {
        files.seek(state.handle, seekAt(kSectionsAt + kSectionStride * i), kFromStart);
        uint8_t section[kSectionRead];
        files.read(state.handle, section, kSectionRead);
        if (vc2in::s16(section, kOnAt) != kOn) continue;
        state.sectionAt = fileWord(section, kPlaceAt);
        files.seek(state.handle, seekAt(state.sectionAt), kFromStart);
        switch (fileHalf(section, kNumberAt)) {
        case 1: setSection(kVelocity, state, files, words, sets); break;
        case 2: setSection(kKey, state, files, words, sets); break;
        case 3: aliasSection(state, files, words, sets); break;
        default: continue;
        }
        progress.parse = static_cast<uint16_t>(progress.parse + kSectionStep);
    }
    files.close(state.handle);
    progress.parse = kParseDone;
    return kVc2IndexParsed;
}
