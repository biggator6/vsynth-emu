#pragma once
// xt_vc2waves' rig for the store's pool (v501, moved out of Vc2WavePoolCheck.cpp for the records' part, v502): rom.4's
// OS allocator answered from the check's memory, zeroed as the port's (the 24 bytes after it too), its lock and the
// OS's list doing nothing, and the check's two vtables, whose moved method (the object 0 or 0x30 bytes in) logs where
// it was called and how far; the port's host the same.  XT_VC2WAVES_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2WavePool.h"
#include <utility>
#include <vector>

inline constexpr uint32_t kVc2PoolMemoryAt = 0x8C100000, kVc2PoolMemoryMost = 0x10000;   // the check's OS memory
inline constexpr uint32_t kVc2PoolVtables = 0x8C030200, kVc2PoolVtableBytes = 0x40;      // the check's vtables
inline constexpr uint32_t kVc2PoolAdjusts[] = { 0, 0x30 };

struct Vc2PoolLogs {
    std::vector<std::pair<uint32_t, int32_t>> moved;   // where each object was told, and how far
    std::vector<uint32_t> os;                          // the OS's calls: what each asked
};

// The port's host: the OS's memory at the same place, and a moved block's object found through its vtable as rom.4's
// call finds it, the check's vtables' adjust
class Vc2PoolCheckHost : public Vc2PoolHost {
public:
    Vc2WavePool& pool;
    Vc2PoolLogs logs;
    explicit Vc2PoolCheckHost(Vc2WavePool& p) : pool(p) {}
    uint32_t osAlloc(uint32_t bytes) override {
        logs.os.push_back(bytes);
        return bytes <= kVc2PoolMemoryMost ? kVc2PoolMemoryAt : 0;
    }
    void osFree(uint32_t at) override { logs.os.push_back(at); }
    void moved(uint32_t block, int32_t by) override {
        const uint32_t k = (pool.word(block + 0x14) - kVc2PoolVtables) / kVc2PoolVtableBytes;
        logs.moved.emplace_back(block + (k < 2 ? kVc2PoolAdjusts[k] : 0), by);
    }
};

// rom.4's side: the OS's allocator and lock hooked (and the OS's list 0x893111b0, which the store's set-up joins), the
// vtables and their methods logging
inline void vc2HookPoolRom(Vc2Fw& fw, Vc2PoolLogs& logs) {
    constexpr uint32_t kOsAlloc = 0x89300770, kOsFree = 0x89300840, kLock = 0x893018b0, kUnlock = 0x893018c0;  // [FW]
    constexpr uint32_t kOsList = 0x893111b0;       // [FW]
    constexpr uint32_t kLockHolder = 0x8992e208;   // [FW] the OS's object its lock's semaphore is read from (+0x2c)
    constexpr uint32_t kOsObject = 0x8C030000, kMethods = 0x8C030300, kAdjustAt = 0x18, kMethodAt = 0x20;
    constexpr uint32_t kOverhang = 24;   // the port's memory past the OS's (Vc2WavePool.h)
    fw.cpu.wr32(kLockHolder, kOsObject);
    for (uint32_t k = 0; k < 2; ++k) {
        fw.cpu.wr32(kVc2PoolVtables + kVc2PoolVtableBytes * k + kAdjustAt, kVc2PoolAdjusts[k]);
        fw.cpu.wr32(kVc2PoolVtables + kVc2PoolVtableBytes * k + kMethodAt, kMethods + 0x10 * k);
        fw.cpu.hook(kMethods + 0x10 * k, [&logs](Sh4& c) {
            logs.moved.emplace_back(c.r[4], static_cast<int32_t>(c.r[5]));
            c.ret(0);
        });
    }
    fw.cpu.hook(kOsAlloc, [&logs](Sh4& c) {
        logs.os.push_back(c.r[4]);
        if (c.r[4] <= kVc2PoolMemoryMost) {
            const std::vector<uint8_t> zeros(c.r[4] + kOverhang);
            c.write(kVc2PoolMemoryAt, zeros.data(), zeros.size());
        }
        c.ret(c.r[4] <= kVc2PoolMemoryMost ? kVc2PoolMemoryAt : 0);
    });
    fw.cpu.hook(kOsFree, [&logs](Sh4& c) {
        logs.os.push_back(c.r[4]);
        c.ret(0);
    });
    for (const uint32_t nothing : { kLock, kUnlock, kOsList })
        fw.cpu.hook(nothing, [](Sh4& c) { c.ret(0); });
}
