// xt_vc2cosm's seventh part's slots: see Vc2CosmDspCheck.h (v264).  Every address marked [FW] is rom.4's.
// XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmDspCheck.h"
#include "Vc2CosmNoteOnDraws.h"
#include <bit>

namespace {
constexpr uint32_t kPoolsAt = 0x89999294, kSlotBytes = 20;   // [FW] the pools' slots, kDspSlots of them to a pool
constexpr uint32_t kBuffersAt = kNoteOnObjectAt + 0x60, kSlotAt = kNoteOnObjectAt + 0x144;
// Four buffers of RAM for the stale words an exhausted pool hands rom.4, each after the words its line 2 can reach
constexpr uint32_t kStaleAt = 0x8C080000, kStaleStride = 0x3800, kStaleStackWords = 256;
constexpr uint32_t kEarlyBytes = Vc2CosmPools::kEarlyFloats * sizeof(float);

uint32_t staleBuffer(uint32_t k) { return kStaleAt + kEarlyBytes + kStaleStride * k; }
} // namespace

void vc2DspHoldSlots(Vc2Fw& fw, Vc2CosmPools& pools, uint32_t pool, int taken) {
    for (int i = 0; i < kDspSlots; ++i) {
        const uint32_t inUse = i < taken ? 1 : 0;
        pools.w.pools[pool][(size_t)i].inUse = inUse;
        fw.cpu.wr32(kPoolsAt + (pool * (uint32_t)kDspSlots + (uint32_t)i) * kSlotBytes, inUse);
    }
}

// The slot before's ends, and in front of slot 0 the last slot's buffers 0 and 2, as each pool's buffers lie end to end
// in rom.4's memory
void vc2DspDrawEnds(Vc2Fw& fw, Vc2CosmPools& pools, uint32_t pool, int slot, std::mt19937& rng) {
    const int before = slot == 0 ? kDspSlots - 1 : slot - 1;
    for (int line : { 1, 3 }) {
        const int k = slot == 0 ? line - 1 : line;
        std::vector<float>& mine = pools.buffer(pool, before, k);
        const uint32_t at = pools.w.pools[pool][(size_t)before].buffers[(size_t)k];
        for (int i = Vc2CosmPools::kBufferFloats - Vc2CosmPools::kEarlyFloats; i < Vc2CosmPools::kBufferFloats; ++i) {
            mine[(size_t)i] = 1.0e7f * (2.0f * vc2Unit(rng) - 1.0f);
            fw.cpu.wr32(at + 4 * (uint32_t)i, std::bit_cast<uint32_t>(mine[(size_t)i]));
        }
    }
}

// rom.4's allocator doesn't test for a full pool: it takes four words of its stack as the buffers and clears them.  So
// those words are four buffers here, in any rotation of them, with nothing but zeros in front of each
void vc2DspDrawStaleStack(Vc2Fw& fw) {
    for (uint32_t k = 0; k < kStaleStackWords; ++k) fw.cpu.wr32(kVc2Sp - 4 * (k + 1), staleBuffer(k % 4));
    const std::vector<uint8_t> zeros(kStaleStride, 0);
    for (uint32_t k = 0; k < 4; ++k) fw.cpu.write(kStaleAt + kStaleStride * k, zeros.data(), zeros.size());
}

std::string vc2DspStaleDifference(Vc2Fw& fw, const Vc2CosmObjectWords& o) {
    if ((int32_t)fw.cpu.rd32(kSlotAt) != -1 || o.slot != -1) return "a slot from an exhausted pool";
    uint32_t seen = 0;
    for (uint32_t k = 0; k < 4; ++k)
        for (uint32_t s = 0; s < 4; ++s) seen |= fw.cpu.rd32(kBuffersAt + 4 * k) == staleBuffer(s) ? 1u << s : 0u;
    return seen == 0xf ? "" : "the stale words didn't reach the allocator";
}
#endif
