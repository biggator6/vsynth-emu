// The VC-2's wave store's records and chunks: see Vc2WaveRecords.h (v502).  The words are the pool's, little-endian;
// a key's list is the key mod 128 with the dividend's sign (C's %), none for a negative key, as rom.4's code takes it.
// A class's destructor writes its own vtable, then its base's: the port writes the base's, which is what stays.
#include "Vc2WaveRecords.h"
#include <algorithm>
#include <iterator>

namespace {
// [FW] the classes' vtables: the base every object ends with, a record's, the data's
constexpr uint32_t kBaseVtable = 0x898ca8bc, kRecordVtable = 0x898ca8e0, kDataVtable = 0x898ca9b8;
// [FW] the plain chunks' classes: the chunk's number, its slot in the record, its vtable
struct PlainClass {
    uint32_t number, slot, vtable;
};
constexpr PlainClass kPlain[] = { { 1, 0x24, 0x898ca904 },  { 2, 0x28, 0x898ca928 },  { 3, 0x2c, 0x898ca94c },
                                  { 4, 0x30, 0x898ca970 },  { 5, 0x34, 0x898ca994 },  { 6, 0x40, 0x898caa00 },
                                  { 7, 0x3c, 0x898ca9dc },  { 10, 0x44, 0x898caa24 }, { 12, 0x4c, 0x898caa48 } };
// [FW] an object's vtable and a chunk's record; a record's node (previous, next, key) and slots; its bytes and halves
constexpr uint32_t kVtableAt = 0x14, kOwnerAt = 0x18, kNodeAt = 0x18, kPrev = 0, kNext = 4, kKey = 8;
constexpr uint32_t kSlotsAt = 0x24, kSlots = 11, kBytesAt = 0x50, kZeroedBytes = 6;   // +0x50, +0x51, +0x52, +0x54
constexpr uint32_t kRecordBytes = 88, kChunkBytes = 28;
// [FW] the data's class: 64 bytes before its data, 32 more asked for; its seven band pointers from +0x1c, the first its
// data; +0x38 which slot it hangs in (0 the record's +0x38, else +0x48), +0x3c a word 0; 32 bytes zeroed after the extra
constexpr uint32_t kDataBytes = 64, kDataExtra = 32, kBandsAt = 0x1c, kBands = 7, kWhichAt = 0x38, kDataZeroAt = 0x3c;
constexpr uint32_t kDataTail = 32, kDataFirst = 0x38, kDataSecond = 0x48, kDataTagAt = 0x10;
constexpr uint32_t kDataNumber = 8, kSecondNumber = 11, kNoChunk = 9, kChunks = 12;   // [FW] 0x89353ef0's numbers
constexpr int32_t kBuckets = 128;

const PlainClass* plainOf(uint32_t number) {
    const auto it = std::ranges::find(kPlain, number, &PlainClass::number);
    return it == std::end(kPlain) ? nullptr : &*it;
}

const PlainClass* plainByVtable(uint32_t vtable) {
    const auto it = std::ranges::find(kPlain, vtable, &PlainClass::vtable);
    return it == std::end(kPlain) ? nullptr : &*it;
}
} // namespace

void Vc2WaveRecords::insert(uint32_t node, int32_t key) {
    const int32_t h = key % kBuckets;
    if (h < 0) return;
    uint32_t& head = store_.words.heads[static_cast<size_t>(h)];
    if (head == 0) {
        setWord(node + kNext, 0);
        setWord(node + kPrev, 0);
    } else if (word(head + kPrev) != 0 && word(head + kNext) != 0) {
        setWord(node + kPrev, word(head + kPrev));
        setWord(node + kNext, head);
        setWord(word(head + kPrev) + kNext, node);
        setWord(head + kPrev, node);
    } else {
        setWord(node + kNext, head);
        setWord(node + kPrev, head);
        setWord(head + kNext, node);
        setWord(head + kPrev, node);
    }
    head = node;
    setWord(node + kKey, static_cast<uint32_t>(key));
}

void Vc2WaveRecords::unlink(uint32_t node) {
    const auto key = static_cast<int32_t>(word(node + kKey));
    const int32_t h = key % kBuckets;
    if (h < 0) return;
    uint32_t& head = store_.words.heads[static_cast<size_t>(h)];
    if (head == 0) return;
    const uint32_t prev = word(node + kPrev);
    if (prev == 0 || word(node + kNext) == 0) {
        head = 0;
        return;
    }
    if (static_cast<int32_t>(word(head + kKey)) == key) head = prev;
    if (word(prev + kNext) == node && word(prev + kPrev) == node) {
        setWord(prev + kNext, 0);
        setWord(prev + kPrev, 0);
        return;
    }
    setWord(prev + kNext, word(node + kNext));
    setWord(word(node + kNext) + kPrev, prev);
}

void Vc2WaveRecords::destroy(uint32_t object) {
    const uint32_t vtable = word(object + kVtableAt);
    if (vtable == kRecordVtable) {   // 0x89353500
        unlink(object + kNodeAt);
    } else if (vtable == kDataVtable) {   // 0x893537b0
        const uint32_t slot = word(object + kWhichAt) == 0 ? kDataFirst : kDataSecond;
        setWord(word(object + kOwnerAt) + slot, 0);
    } else if (const PlainClass* c = plainByVtable(vtable)) {   // 0x89353630 + 0x40 a class
        setWord(word(object + kOwnerAt) + c->slot, 0);
    }
    setWord(object + kVtableAt, kBaseVtable);
    pool_.freeObject(object);
}

int32_t Vc2WaveRecords::make(int32_t key, uint8_t type) {
    if (recordOf(key) != 0) remove(key);
    const uint32_t record = pool_.allocObject(kRecordBytes, static_cast<uint32_t>(type) + 1);
    if (record == 0) return kVc2RecordNoRoom;
    setWord(record + kVtableAt, kRecordVtable);
    for (uint32_t i = 0; i < kSlots; ++i) setWord(record + kSlotsAt + 4 * i, 0);
    std::fill_n(pool_.bytesAt(record + kBytesAt), kZeroedBytes, 0);
    insert(record + kNodeAt, key);
    return kVc2RecordMade;
}

int32_t Vc2WaveRecords::makeChunk(int32_t key, uint32_t n, uint32_t extra) {
    const uint32_t record = recordOf(key);
    if (record == 0 || n - 1 >= kChunks || n == kNoChunk) return kVc2ChunkNone;
    const bool data = n == kDataNumber || n == kSecondNumber;
    const PlainClass* plain = plainOf(n);
    const uint32_t slot = data ? (n == kDataNumber ? kDataFirst : kDataSecond) : plain->slot;
    if (const uint32_t old = word(record + slot); old != 0) destroy(old);
    const uint32_t chunk =
        data ? pool_.allocObject(kDataBytes, extra + kDataExtra) : pool_.allocObject(kChunkBytes, extra);
    if (chunk == 0) return kVc2RecordNoRoom;
    setWord(chunk + kVtableAt, data ? kDataVtable : plain->vtable);
    if (!data) {
        setWord(chunk + kOwnerAt, record);
        setWord(record + slot, chunk);
        return kVc2RecordMade;
    }
    setWord(chunk + kDataZeroAt, 0);
    setWord(chunk + kBandsAt, chunk + kDataBytes);
    for (uint32_t i = 1; i < kBands; ++i) setWord(chunk + kBandsAt + 4 * i, 0);
    setWord(chunk + kOwnerAt, record);
    const bool second = word(record + kDataFirst) != 0;   // 0x89353ef0: the second slot when the first holds one
    setWord(chunk + kWhichAt, second ? 1 : 0);
    setWord(record + (second ? kDataSecond : kDataFirst), chunk);
    const uint32_t tail = chunk + kDataBytes + ((word(chunk + kDataTagAt) - kDataExtra) & ~1u);   // 0x89353790
    std::fill_n(pool_.bytesAt(tail), kDataTail, 0);
    return kVc2RecordMade;
}

int32_t Vc2WaveRecords::remove(int32_t key) {
    const uint32_t record = recordOf(key);
    if (record == 0) return kVc2RecordNone;
    for (uint32_t i = 0; i < kSlots; ++i)
        if (const uint32_t chunk = word(record + kSlotsAt + 4 * i); chunk != 0) destroy(chunk);
    destroy(record);
    return kVc2RecordMade;
}

void Vc2WaveRecords::recordMoved(uint32_t record, int32_t by) {
    for (uint32_t i = 0; i < kSlots; ++i)
        if (const uint32_t chunk = word(record + kSlotsAt + 4 * i); chunk != 0)
            setWord(chunk + kOwnerAt, word(chunk + kOwnerAt) + static_cast<uint32_t>(by));
    const uint32_t node = record + kNodeAt;
    const int32_t h = static_cast<int32_t>(word(node + kKey)) % kBuckets;
    if (h < 0) return;
    uint32_t& head = store_.words.heads[static_cast<size_t>(h)];
    if (head == 0) return;
    if (head == node - static_cast<uint32_t>(by)) head = node;
    const uint32_t prev = word(node + kPrev), next = word(node + kNext);
    if (prev == 0 || next == 0) return;
    setWord(prev + kNext, node);
    setWord(next + kPrev, node);
}

void Vc2WaveRecords::moved(uint32_t block, int32_t by) {
    const uint32_t vtable = word(block + kVtableAt), shift = static_cast<uint32_t>(by);
    const auto addTo = [this, shift](uint32_t at) { setWord(at, word(at) + shift); };
    if (vtable == kRecordVtable) {   // 0x89353540
        recordMoved(block, by);
    } else if (vtable == kDataVtable) {   // 0x89353800
        for (uint32_t i = 0; i < kBands; ++i) addTo(block + kBandsAt + 4 * i);
        addTo(word(block + kOwnerAt) + (word(block + kWhichAt) == 0 ? kDataFirst : kDataSecond));
    } else if (const PlainClass* c = plainByVtable(vtable)) {   // 0x89353660 + 0x40 a class
        addTo(word(block + kOwnerAt) + c->slot);
    } else {
        os_.moved(block, by);
    }
}
