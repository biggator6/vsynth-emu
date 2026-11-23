// xt_vc2waves' fourth part's lookups (v503): the port's on what rom.4's load left (Vc2LoadedWaves: the store, the sets
// and the pool, what Vc2SetReads' waveStore(), waveSets() and waveMemory() give PCM's start) against rom.4's own on the
// same machine.  Whether a record is loaded (0x89351840) and a set's index (0x893520a0) for every key below 0x200, the
// library's (2999…3199) and the sampling record's (0x1000000, 0x1000001); a wave's words by its Wave Number
// (0x8939cd30) for every number below 0x200; by a set's index (0x8939cba0) for every index to the velocity set's count
// and two past it, at every key (velocity 100) and every velocity (key 60), then [cases] drawn ones.  Each call's
// answer, the words written (both sides given the same marks first) and, after it, the sets' buffers.
// XT_VC2WAVES_MAIN only.
#ifdef XT_VC2WAVES_MAIN
#include "Vc2WavesCheck.h"
#include <array>
#include <random>

namespace {
constexpr uint32_t kLoaded = 0x89351840, kSetIndex = 0x893520a0, kByNumber = 0x8939cd30, kBySet = 0x8939cba0;   // [FW]
constexpr uint32_t kBuffersAt = 0x89989754;   // [FW] the sets' buffers
constexpr uint32_t kWaveAt = 0x8C0F2000;      // the words' place in the load's RAM
constexpr size_t kWaveBytes = 0x60;           // [FW] the words a lookup writes: +0…+0x5c
constexpr uint8_t kMark = 0xa5;
constexpr uint64_t kLookupSteps = 10'000'000;
struct KeyRun {
    uint32_t from, to;
};
constexpr KeyRun kKeys[] = { { 0, 0x200 }, { 2999, 3200 }, { 0x1000000, 0x1000002 } };
constexpr uint32_t kNumbers = 0x200, kPastCount = 2, kValues = 128;
constexpr uint8_t kSweepVelocity = 100, kSweepKey = 60;

struct Lookups {
    Sh4& c;
    Vc2LoadedWaves& w;
    Vc2WavesTally& tally;
    long calls = 0;
};
using Words = std::array<uint8_t, kWaveBytes>;

// One lookup on both sides: rom.4's routine `at` on `args`, the port's `mine` writing into its words; its outcome
// said under `where`
template <class Mine>
void both(Lookups& l, uint32_t at, std::initializer_list<uint32_t> args, Mine mine, const std::string& where) {
    Words wave;
    wave.fill(kMark);
    l.c.write(kWaveAt, wave.data(), wave.size());
    uint32_t r0 = 0;
    const bool ok = l.c.call(at, args, Vc2WaveBoot::kStack, &r0, kLookupSteps);
    const int64_t ours = mine(wave.data());
    ++l.calls;
    l.tally.outcome(vc2CallOutcome(l.c, ok, { ours, (int32_t)r0 }, [&] {
                        const std::string d = vc2BytesDifference(l.c, kWaveAt, wave.data(), wave.size(), "the words");
                        if (!d.empty()) return d;
                        const auto& b = l.w.sets.buffers;
                        return vc2BytesDifference(l.c, kBuffersAt, &b, sizeof b, "the sets' buffers");
                    }),
                    where);
}

void byKey(Lookups& l, uint32_t key) {
    const std::string at = " on key " + std::to_string(key);
    both(l, kLoaded, { key }, [&](uint8_t*) { return l.w.store.loadedRecord(key, l.w) != 0 ? 1 : 0; },
         "0x89351840" + at);
    both(l, kSetIndex, { key }, [&](uint8_t*) { return l.w.store.setIndex(key, l.w); }, "0x893520a0" + at);
}

void byNumber(Lookups& l, uint32_t number) {
    both(l, kByNumber, { number, kWaveAt },
         [&](uint8_t* wave) { return vc2WaveWords(l.w.store, (uint16_t)number, wave, l.w) ? 1 : 0; },
         "0x8939cd30 on number " + std::to_string(number));
}

void bySet(Lookups& l, uint32_t index, uint32_t key, uint32_t velocity) {
    both(l, kBySet, { index, kWaveAt, key, velocity },
         [&](uint8_t* wave) {
             return l.w.sets.waveWords((uint16_t)index, wave, (uint8_t)key, (uint8_t)velocity, l.w.store, l.w)
                        ? 1
                        : 0;
         },
         "0x8939cba0 on " + std::to_string(index) + ", key " + std::to_string(key) + ", velocity " +
             std::to_string(velocity));
}
} // namespace

long vc2WaveLookups(Sh4& c, Vc2LoadedWaves& w, const Vc2Run& run, Vc2WavesTally& tally) {
    Lookups l { c, w, tally };
    for (const KeyRun& r : kKeys)
        for (uint32_t key = r.from; key < r.to && !tally.stopped; ++key) byKey(l, key);
    for (uint32_t number = 0; number < kNumbers && !tally.stopped; ++number) byNumber(l, number);
    for (uint32_t index = 0; index <= w.sets.counts.velocity + kPastCount && !tally.stopped; ++index)
        for (uint32_t v = 0; v < kValues; ++v) {
            bySet(l, index, v, kSweepVelocity);
            bySet(l, index, kSweepKey, v);
        }
    std::mt19937 rng((uint32_t)run.seed);
    for (int k = 0; k < run.cases && !tally.stopped; ++k)
        bySet(l, rng() % (w.sets.counts.velocity + 1u + kPastCount), rng() % kValues, rng() % kValues);
    return l.calls;
}
#endif
