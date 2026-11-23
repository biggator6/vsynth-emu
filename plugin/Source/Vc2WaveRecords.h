#pragma once
// The VC-2's wave store's records and chunks (v502): what rom.4's loaders build in the store (Vc2WaveStore, v398) and
// its pool (Vc2WavePool, v501).  A record is an 88-byte pool object: its vtable 0x898ca8e0 at +0x14, its node at +0x18 on
// the store's lists (the previous node, the next, the key), its chunks' slots +0x24…+0x4c, a byte at +0x50 and +0x51,
// halfwords at +0x52 and +0x54.  A chunk is a pool object of 28 bytes and its data: its class's vtable at +0x14, its
// record at +0x18, and for the data's class (64 bytes before its data) seven band pointers from +0x1c, the first at its
// data, and at +0x38 which of the record's two slots it hangs in.  Ported: a record's making 0x89353b40 (the store's
// record of the key deleted first), a chunk's 0x89353ef0 (the slot's old chunk destroyed first), a record's deletion
// 0x89354490 with every class's destructor, the lists' insert 0x893533d0 and unlink 0x89353460, and every class's moved
// method, which the pool calls when it moves an object (Vc2PoolHost::moved, here; an object of no class of the store's
// goes on to the OS's host).  Bit for bit: xt_vc2waves' third part.  HARDWARE_FIRMWARE.md, "The VC-2's wave store's
// records".  The notes' task.
#include "Vc2WavePool.h"
#include "Vc2WaveStore.h"
#include <cstdint>

inline constexpr int32_t kVc2RecordMade = 1, kVc2RecordNoRoom = -1, kVc2RecordNone = -3, kVc2ChunkNone = -5;  // [FW]

class Vc2WaveRecords : public Vc2PoolHost {
public:
    // The store and its pool, and the OS's side: its allocator, and the moved method of any object not the store's
    Vc2WaveRecords(Vc2WaveStore& store, Vc2WavePool& pool, Vc2PoolHost& os) : store_(store), pool_(pool), os_(os) {}
    int32_t make(int32_t key, uint8_t type);                      // 0x89353b40: 1, or -1 with no room
    int32_t makeChunk(int32_t key, uint32_t n, uint32_t extra);   // 0x89353ef0: 1, -1 with no room, -5 for none
    int32_t remove(int32_t key);                                  // 0x89354490: 1, or -3 with no record
    // Vc2PoolHost: the OS's allocator through `os`, a moved object by its class
    uint32_t osAlloc(uint32_t bytes) override { return os_.osAlloc(bytes); }
    void osFree(uint32_t at) override { os_.osFree(at); }
    void moved(uint32_t block, int32_t by) override;

private:
    Vc2WaveStore& store_;
    Vc2WavePool& pool_;
    Vc2PoolHost& os_;
    uint32_t word(uint32_t at) const { return pool_.word(at); }
    void setWord(uint32_t at, uint32_t v) { pool_.setWord(at, v); }
    uint32_t recordOf(int32_t key) const { return store_.recordOf(static_cast<uint32_t>(key), pool_); }
    void insert(uint32_t node, int32_t key);          // 0x893533d0
    void unlink(uint32_t node);                       // 0x89353460
    void destroy(uint32_t object);                    // its class's destructor, given 3: freed
    void recordMoved(uint32_t record, int32_t by);    // 0x89353540
};
