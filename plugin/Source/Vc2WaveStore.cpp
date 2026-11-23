// The VC-2's wave store: see Vc2WaveStore.h (v398).  The wave's words stored as the part's settings' readers store a
// table's (Vc2PartWords, a word as the register holds it), read as a converter reads its input (vc2in).
#include "Vc2WaveStore.h"
#include "Vc2InputReads.h"
#include "Vc2PartSettingsWords.h"
#include <cstring>
#include <iterator>

using Vc2PartWords::put16;
using Vc2PartWords::put32;

namespace {
// [FW] a record's words: its node at +0x18 (at the node's +4 the next node's address, at its +8 the key); the blocks
// 0x89351ad0 gives by chunk (2 the vpsc's at +0x28, 3 the vpno's +0x2c, 4 the vplp's +0x30, 5 the vpdt's +0x34, 10 the
// cached loop events' +0x44), the data's block 0x89351ff0 reads (+0x38), a set's block (+0x4c) and the halfword +0x52
constexpr uint32_t kNodeAt = 0x18, kNextAt = 4, kKeyAt = 8, kBuckets = 128;
constexpr uint32_t kVpscAt = 0x28, kDataAt = 0x38, kSetAt = 0x4c, kBusyAt = 0x52;
constexpr uint32_t kBlockData = 28;    // [FW] a block's header, before its chunk's data
constexpr uint32_t kFramesAt = 0x20;   // [FW] the vpsc's frames in its block (its data's word 1)
constexpr int32_t kNoRecord = -1, kNoSet = -2;
constexpr uint32_t kNumberBase = 3000;   // [FW] 0x8939cd30's key: the Wave Number less 1, plus 3000
// [FW] the wave's words: found +0, the level curve's two +4 and +6 and the root's offset +5, the type +7, the root key
// +8, the stereo flag's word +0xc, the loop switch +0x10, a halfword 0 +0x12, the events' count +0x14, a word 0 +0x18,
// the frames +0x1c, the vpsc's, vpno's and vplp's data +0x20…+0x28 (as +0x4c…+0x54 hold them), the data's words
// +0x30…+0x44
constexpr int kFoundAt = 0, kVelocityLevelAt = 4, kRootOffsetAt = 5, kKeyLevelAt = 6, kTypeAt = 7, kRootAt = 8;
constexpr int kStereoAt = 0xc, kLoopSwitchAt = 0x10, kZeroHalfAt = 0x12, kEventsAt = 0x14, kZeroAt = 0x18;
constexpr int kFramesWordAt = 0x1c, kChunksAt = 0x20, kKeptAt = 0x4c, kKept = 3, kDataWordsAt = 0x30;
// [FW] what the wave's words take from the vpsc's data (+1 the type, +2 stereo, +0xa the root key), the vpno's (+4 its
// count; 2 with no vpno) and the vplp's (+0x1a the loop switch); the level curve's two from the store, 127 each
constexpr uint32_t kVpscTypeAt = 1, kVpscStereoAt = 2, kVpscRootAt = 0xa, kVpnoCountAt = 4, kVplpSwitchAt = 0x1a;
constexpr uint32_t kNoVpno = 2;
constexpr uint8_t kLevelTop = 127;

// [FW] 0x8939cd30's calls of 0x89351ad0 in its order, the wave's word each fills and the record's block: 5, 2, 3, 4, 10
struct ChunkRow {
    int waveAt;
    uint32_t recordAt;
};
constexpr ChunkRow kChunks[] = { { 0x2c, 0x34 }, { 0x4c, 0x28 }, { 0x50, 0x2c }, { 0x54, 0x30 }, { 0x58, 0x44 } };
// [FW] its calls of 0x89351ff0: the data's words 0, 2…6, into +0x30…+0x44
constexpr uint32_t kDataWords[] = { 0, 2, 3, 4, 5, 6 };

// 0x89351ad0 on the record: a block's chunk's data, 0 for none
uint32_t chunkOf(uint32_t record, uint32_t at, const Vc2WaveMemory& m) {
    const uint32_t block = m.word(record + at);
    return block == 0 ? 0 : block + kBlockData;
}

// 0x89351ff0 on the record: the data's word i
uint32_t dataWord(uint32_t record, uint32_t i, const Vc2WaveMemory& m) {
    return m.word(m.word(record + kDataAt) + kBlockData + 4 * i);
}
} // namespace

uint16_t Vc2WaveMemory::half(uint32_t at) const { return static_cast<uint16_t>(byte(at) | byte(at + 1) << 8); }

uint32_t Vc2WaveMemory::word(uint32_t at) const { return half(at) | static_cast<uint32_t>(half(at + 2)) << 16; }

// 0x89353130: the list of the key mod 128, a node alone (its link 0) or a ring of them, for the first of that key
uint32_t Vc2WaveStore::recordOf(uint32_t key, const Vc2WaveMemory& m) const {
    const uint32_t k = words.key >= 0 ? static_cast<uint32_t>(words.key) : key;
    const uint32_t head = words.heads[k % kBuckets];
    for (uint32_t node = head; node != 0;) {
        if (m.word(node + kKeyAt) == k) return node - kNodeAt;
        node = m.word(node + kNextAt);
        if (node == head) break;
    }
    return 0;
}

uint32_t Vc2WaveStore::loadedRecord(uint32_t key, const Vc2WaveMemory& m) const {
    const uint32_t record = recordOf(key, m);
    if (record == 0 || m.word(record + kSetAt) != 0) return 0;
    return m.half(record + kBusyAt) == 0 ? record : 0;
}

int32_t Vc2WaveStore::setIndex(uint32_t key, const Vc2WaveMemory& m) const {
    const uint32_t record = recordOf(key, m);
    if (record == 0) return kNoRecord;
    const uint32_t set = m.word(record + kSetAt);
    return set == 0 ? kNoSet : static_cast<int32_t>(m.word(set + kBlockData));
}

bool vc2WaveLoadedWords(const Vc2WaveStore& store, uint32_t key, uint8_t* wave, const Vc2WaveMemory& m,
                        bool fromStore) {
    const uint32_t record = store.loadedRecord(key, m);
    if (record == 0) return false;
    for (const ChunkRow& c : kChunks) put32(wave + c.waveAt, chunkOf(record, c.recordAt, m));
    for (int i = 0; i < static_cast<int>(std::size(kDataWords)); ++i)
        put32(wave + kDataWordsAt + 4 * i, dataWord(record, kDataWords[i], m));
    std::memmove(wave + kChunksAt, wave + kKeptAt, sizeof(uint32_t) * kKept);
    const uint32_t vpsc = vc2in::u32(wave, kChunksAt), vpno = vc2in::u32(wave, kChunksAt + 4);
    if (fromStore) {
        wave[kVelocityLevelAt] = kLevelTop;
        wave[kRootOffsetAt] = 0;
        wave[kKeyLevelAt] = kLevelTop;
        wave[kTypeAt] = m.byte(vpsc + kVpscTypeAt);
    }
    wave[kRootAt] = m.byte(vpsc + kVpscRootAt);
    put32(wave + kStereoAt, m.byte(vpsc + kVpscStereoAt));
    put16(wave + kZeroHalfAt, 0);
    put16(wave + kEventsAt, fromStore && vpno == 0 ? kNoVpno : m.word(vpno + kVpnoCountAt));
    put32(wave + kZeroAt, 0);
    put32(wave + kFramesWordAt, m.word(m.word(record + kVpscAt) + kFramesAt));
    wave[kLoopSwitchAt] = m.byte(vc2in::u32(wave, kChunksAt + 8) + kVplpSwitchAt);
    return true;
}

bool vc2WaveWords(const Vc2WaveStore& store, uint16_t number, uint8_t* wave, const Vc2WaveMemory& m) {
    if (number == 0) return false;
    put32(wave + kFoundAt, 1);
    return vc2WaveLoadedWords(store, static_cast<uint16_t>(number - 1 + kNumberBase), wave, m, true);
}
