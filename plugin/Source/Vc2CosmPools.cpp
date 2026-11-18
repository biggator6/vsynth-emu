// rom.4's COSM buffer pools: see Vc2CosmPools.h (v250).  Every address is rom.4's.
#include "Vc2CosmPools.h"
#include "Vc2CosmTables.h"
#include <algorithm>

namespace {
// [FW] each pool's four buffers, the first slot's, and the step to the next slot's (3,267 floats)
constexpr std::array<std::array<uint32_t, 4>, Vc2CosmPoolWords::kPools> kBases { {
    { 0x89e4b084, 0x89e979a4, 0x89ee42c4, 0x89f30be4 }, { 0x89f7d504, 0x89fc9e24, 0x8a016744, 0x8a063064 } } };
constexpr uint32_t kSlotStep = 0x330c;
constexpr uint32_t kDeletes = 1;   // [FW] the destructor's flag bit for operator delete
} // namespace

Vc2CosmPools::Vc2CosmPools() {
    for (auto& pool : own_)
        for (auto& slot : pool)
            for (std::vector<float>& b : slot) b.assign(kBufferFloats, 0.0f);
}

void Vc2CosmPools::init() {
    const Vc2CosmObjectPool& t = vc2CosmTables().object;
    w.after[0] = t.poolFloats[1];
    w.after[1] = t.poolFloats[2];
    w.before = t.poolFloats[0];
    for (uint32_t p = 0; p < Vc2CosmPoolWords::kPools; ++p)
        for (uint32_t i = 0; i < Vc2CosmPoolWords::kSlots; ++i) {
            Vc2CosmSlot& s = w.pools[p][i];
            s.inUse = 0;
            for (size_t k = 0; k < s.buffers.size(); ++k) s.buffers[k] = kBases[p][k] + kSlotStep * i;
        }
}

int32_t Vc2CosmPools::take(uint32_t pool, std::array<uint32_t, 4>& buffers) {
    for (int32_t i = 0; i < Vc2CosmPoolWords::kSlots; ++i) {
        Vc2CosmSlot& s = w.pools[pool][(size_t)i];
        if (s.inUse != 0) continue;
        s.inUse = 1;
        buffers = s.buffers;
        return i;
    }
    return -1;
}

void Vc2CosmPools::release(uint32_t pool, int32_t slot) { w.pools[pool][(size_t)slot].inUse = 0; }

std::vector<float>& Vc2CosmPools::buffer(uint32_t pool, int32_t slot, int k) {
    return own_[pool][(size_t)slot][(size_t)k];
}

const float* Vc2CosmPools::earlyOf(uint32_t pool, int32_t slot, int k) const {
    const bool first = slot == 0;
    const auto before = static_cast<size_t>(first ? Vc2CosmPoolWords::kSlots - 1 : slot - 1);
    return own_[pool][before][static_cast<size_t>(first ? k - 1 : k)].data() + kBufferFloats - kEarlyFloats;
}

void Vc2CosmPools::keepEnd(uint32_t pool, int32_t slot, int k, const float* ring, int length) {
    const int from = kBufferFloats - kEarlyFloats;
    std::copy(ring + from, ring + length, buffer(pool, slot, k).begin() + from);
}

bool vc2CosmDestroy(Vc2CosmObjectWords& o, Vc2CosmPools& pools, uint32_t flags) {
    o.vtable = vc2CosmVtable();
    if (o.slot != -1 && o.pool < Vc2CosmPoolWords::kPools) pools.release(o.pool, o.slot);
    return (flags & kDeletes) != 0;
}
