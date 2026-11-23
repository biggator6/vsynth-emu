// xt_vc2sets' eighth part's wave store (v398): the store's words, its records with their nodes and their chunks'
// blocks drawn, the words into the port's copy (written into rom.4's memory with the OSC blocks before each event,
// compared after), the rest into rom.4's memory, which both sides read; and the Wave Numbers about the store's that an
// OSC record's draws and the store's events aim with.  Not drawn: a ring that doesn't come back to its head, a link of 0
// in a ring of more than one, a record's vpsc, vplp or data block of 0 (rom.4 reads each untested where the store
// holds the wave loaded, faulting at 0 + the read).  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetOscBlockCheck.h"
#include "Vc2WaveStore.h"
#include <vector>

namespace {
// [FW] a record's words: its node (+0x18; the node's link at its +4, the key at its +8), the chunks' blocks (the
// vpsc's +0x28, the vpno's +0x2c, the vplp's +0x30, the vpdt's +0x34, the data's +0x38, the loop events' +0x44), a
// set's block +0x4c and the halfword +0x52; a block's data 28 bytes in; a Wave Number's key less 1 plus 3000
constexpr uint32_t kNodeAt = 0x18, kLinkAt = 4, kKeyAt = 0x20;
constexpr uint32_t kVpscAt = 0x28, kVpnoAt = 0x2c, kVplpAt = 0x30, kVpdtAt = 0x34, kDataAt = 0x38, kLoopEventsAt = 0x44;
constexpr uint32_t kSetAt = 0x4c, kBusyAt = 0x52, kBlockData = 28, kKeyBase = 3000 - 1, kLists = 128;
// [FW] what the wave's words read in a block's data: the vpsc's type (+1), stereo flag (+2) and frames (+4), the
// vpno's count (+4), the vplp's loop switch (+0x1a); a set's index (+0)
constexpr uint32_t kTypeAt = 1, kStereoAt = 2, kFramesAt = 4, kCountAt = 4, kSwitchAt = 0x1a;
// The Wave Numbers drawn: 0…40, a time in four 128 or 256 up (keys in the same list)
constexpr uint32_t kNumbers = 41, kListStep = 128;

// The blocks by what they're drawn as, a run each: the vpscs', the vplps', the data's, the sets'
struct BlockRun {
    uint32_t first, count;
    void (*steer)(uint8_t* data, std::mt19937& rng);
};

// The vpsc's type one of its six mostly, its stereo flag 0 or 1 mostly, its frames 0 now and then
void vpscSteered(uint8_t* data, std::mt19937& rng) {
    data[kTypeAt] = (uint8_t)(rng() % 4 ? rng() % 6 : rng());
    data[kStereoAt] = (uint8_t)(rng() % 4 ? rng() % 2 : rng());
    if (rng() % 16 == 0) std::memset(data + kFramesAt, 0, sizeof(uint32_t));
}

void vplpSteered(uint8_t* data, std::mt19937& rng) { data[kSwitchAt] = (uint8_t)(rng() % 4 ? rng() % 2 : rng()); }

void noiseKept(uint8_t*, std::mt19937&) {}

// A set's index 0 a time in four, else 1…512 mostly, now and then any word
void setSteered(uint8_t* data, std::mt19937& rng) {
    const uint32_t index = rng() % 4 == 0 ? 0 : rng() % 8 ? 1 + rng() % 512 : (uint32_t)rng();
    std::memcpy(data, &index, sizeof index);
}

constexpr BlockRun kVpscs { 0, 64, vpscSteered }, kVplps { 64, 64, vplpSteered }, kDatas { 128, 32, noiseKept };
constexpr BlockRun kSets { 160, 32, setSteered };

uint32_t blockFromRun(const BlockRun& run, std::mt19937& rng) {
    return kOscBlocksAt + kOscBlockBytes * (run.first + (uint32_t)(rng() % run.count));
}

void waveBlocksDrawn(Vc2Fw& fw, std::mt19937& rng) {
    std::vector<uint8_t> blocks(kOscBlockBytes * kOscBlocks);
    for (uint8_t& b : blocks) b = (uint8_t)rng();
    for (const BlockRun& run : { kVpscs, kVplps, kDatas, kSets })
        for (uint32_t k = run.first; k < run.first + run.count; ++k)
            run.steer(blocks.data() + kOscBlockBytes * k + kBlockData, rng);
    fw.cpu.write(kOscBlocksAt, blocks.data(), blocks.size());
}

// A record of `key`: noise, its blocks from their runs (the vpno's data in the events' region, its count small half
// the time; none half the time), the vpdt's and loop events' 0 now and then, a set's block half the time, the halfword
// +0x52 clear three times in four
void waveRecordDrawn(Vc2Fw& fw, uint32_t at, uint32_t key, std::mt19937& rng) {
    std::array<uint8_t, kOscWaveRecordBytes> noise;
    setsNoise(noise, rng);
    fw.cpu.write(at, noise.data(), noise.size());
    fw.cpu.wr32(at + kKeyAt, key);
    fw.cpu.wr32(at + kVpscAt, blockFromRun(kVpscs, rng));
    const uint32_t vpno = kOscEventsAt + (uint32_t)(rng() % kOscEventsAims);
    fw.cpu.wr32(at + kVpnoAt, rng() % 2 ? 0 : vpno - kBlockData);
    if (rng() % 2) {
        const auto count = (uint32_t)(rng() % 25);
        fw.cpu.write(vpno + kCountAt, &count, sizeof count);
    }
    fw.cpu.wr32(at + kVplpAt, blockFromRun(kVplps, rng));
    fw.cpu.wr32(at + kDataAt, blockFromRun(kDatas, rng));
    for (const uint32_t kept : { kVpdtAt, kLoopEventsAt })
        if (rng() % 4 == 0) fw.cpu.wr32(at + kept, 0);
    fw.cpu.wr32(at + kSetAt, rng() % 2 ? 0 : blockFromRun(kSets, rng));
    if (rng() % 4) fw.cpu.wr16(at + kBusyAt, 0);
}

// The records, each in the list of its key mod 128 (now and then in none): a list a node alone, its link 0 half the
// time or itself, or a ring of them in the records' order; the heads into the store's words
void waveRecordsDrawn(Vc2Fw& fw, Vc2WaveStoreWords& w, std::mt19937& rng) {
    std::array<std::vector<uint32_t>, kLists> lists;
    for (uint32_t r = 0; r < kOscWaveRecords; ++r) {
        const uint32_t record = kOscWaveRecordsAt + kOscWaveRecordBytes * r;
        const uint32_t key = (uint16_t)(oscStoreNumber(rng) + kKeyBase);
        waveRecordDrawn(fw, record, key, rng);
        if (rng() % 8) lists[key % kLists].push_back(record + kNodeAt);
    }
    for (uint32_t h = 0; h < kLists; ++h) {
        const std::vector<uint32_t>& nodes = lists[h];
        w.heads[h] = nodes.empty() ? 0 : nodes[0];
        for (size_t i = 0; i < nodes.size(); ++i) fw.cpu.wr32(nodes[i] + kLinkAt, nodes[(i + 1) % nodes.size()]);
        if (nodes.size() == 1 && rng() % 2) fw.cpu.wr32(nodes[0] + kLinkAt, 0);
    }
}

// The store's own key: not negative a time in eight, then a drawn record's key, any halfword or 0 (a Wave Number of
// 62537's key)
int32_t storeKeyDrawn(std::mt19937& rng) {
    const auto keys = std::to_array<int32_t>({ -1, (int32_t)((uint32_t)rng() | 0x80000000u),
                                               (int32_t)(uint16_t)(oscStoreNumber(rng) + kKeyBase),
                                               (int32_t)(rng() % 0x10000), 0 });
    return keys[rng() % 8 ? rng() % 2 : 2 + rng() % 3];
}
} // namespace

uint16_t oscStoreNumber(std::mt19937& rng) {
    return (uint16_t)(rng() % kNumbers + kListStep * (rng() % 4 ? 0 : 1 + rng() % 2));
}

void oscStoreDrawn(Vc2Fw& fw, Vc2WaveStore& store, std::mt19937& rng, bool all) {
    if (all) {
        std::array<uint8_t, kOscZeroPageBytes> page;
        setsNoise(page, rng);
        if (fw.cpu.ptr(0, 1) == nullptr) fw.cpu.mapRam(0, kOscZeroPageBytes);
        fw.cpu.write(0, page.data(), page.size());
    }
    if (all || rng() % 32 == 0) {
        waveBlocksDrawn(fw, rng);
        waveRecordsDrawn(fw, store.words, rng);
    }
    if (all || rng() % 8 == 0) store.words.key = storeKeyDrawn(rng);
}
#endif
