// xt_vc2waves' pool part's draws (v501, moved out of Vc2WavePoolCheck.cpp at its line limit): a request's size, a
// pool's, and a realloc's that fills a block exactly.  XT_VC2WAVES_MAIN only.
#ifdef XT_VC2WAVES_MAIN
#include "Vc2WavePool.h"
#include "Vc2WavesCheck.h"

namespace {
constexpr uint32_t kNextAt = 4, kStateAt = 8, kUnitsAt = 0xc, kFree = 0;   // [FW] a pool block's header
constexpr uint32_t kUnitRequest = 40;   // [FW] the size law's step: a request of 40q + 1 bytes takes q's units
constexpr size_t kFitsMost = 64;

// The bytes whose size law gives `units` exactly (some units no request gives); 0 for none
uint32_t bytesForUnits(uint32_t units) {
    for (uint32_t q = 0; q <= units; ++q)
        if (Vc2WavePool::unitsFor(kUnitRequest * q + 1) == units) return kUnitRequest * q + 1;
    return 0;
}
} // namespace

uint32_t vc2PoolDrawBytes(std::mt19937& rng, const Vc2WavePool& pool) {
    constexpr uint32_t kSizes[] = { 48, 200, 1000 };
    const uint32_t odd[] = { 0, pool.words.free, pool.words.free + 1 + rng() % 64 };
    if (rng() % 16 == 0) return odd[rng() % 3];
    if (rng() % 4 == 0) return kSizes[rng() % 3];
    return 1 + rng() % std::max<uint32_t>(1, pool.words.size / (rng() % 2 ? 64 : 8));
}

uint32_t vc2PoolDrawSize(std::mt19937& rng) {
    constexpr uint32_t kOdd[] = { 0, kVc2PoolMemoryMost + 1, 0x80000000u };
    return rng() % 16 == 0 ? kOdd[rng() % 3] : 2048 + rng() % (kVc2PoolMemoryMost - 2048 + 1);
}

uint32_t vc2PoolFittingBytes(std::mt19937& rng, const Vc2WavePool& pool, uint32_t block) {
    const uint32_t next = pool.word(block + kNextAt), units = pool.word(block + kUnitsAt);
    const uint32_t joined = units + pool.word(next + kUnitsAt) + 1;
    const bool nextFree = pool.word(next + kStateAt) == kFree;
    if (nextFree && bytesForUnits(joined) && rng() % 2) return bytesForUnits(joined);   // the block and its next, half
    std::vector<uint32_t> fits;
    if (nextFree) fits.push_back(joined);
    uint32_t b = pool.words.base;
    do {
        const uint32_t has = pool.word(b + kUnitsAt);
        if (pool.word(b + kStateAt) == kFree && has > units) fits.push_back(has);
        b = pool.word(b + kNextAt);
    } while (b != pool.words.base && fits.size() < kFitsMost);
    const uint32_t bytes = fits.empty() ? 0 : bytesForUnits(fits[rng() % fits.size()]);
    return bytes ? bytes : vc2PoolDrawBytes(rng, pool);
}
#endif
