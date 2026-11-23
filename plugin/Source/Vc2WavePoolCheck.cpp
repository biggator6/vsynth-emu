// xt_vc2waves' pool part (v501): rom.4's pool routines on its pool at 0x8997c51c against Vc2WavePool, call by call on
// small pools, each case a set-up and a run of drawn calls: alloc, calloc, free, realloc, the largest free block, the
// compaction, the objects' four, now and then a set-up again.  The OS's allocator (0x89300770, 0x89300840) gives both
// the same memory, zeroed, and is logged; its lock (0x893018b0, 0x893018c0) does nothing.  Every block a call gives
// gets noise for its payload and one of two vtables at +0x14 on both sides, whose moved-method (the object 0 or 0x30
// bytes in) logs where it was called and how far.  After each call: the answer, the pool's words, its whole memory and
// both logs.  The draws: pools of 2 to 64 KB (now and then 0, too large to get, or negative), sizes up to an eighth or
// a sixty-fourth of the pool (now and then 0 or past what's free), the extra bytes 0…40.  XT_VC2WAVES_MAIN only.
#ifdef XT_VC2WAVES_MAIN
#include "Vc2WavePool.h"
#include "Vc2WavesCheck.h"
#include <span>

namespace {
constexpr uint32_t kSetUp = 0x89350c10, kAlloc = 0x89350cd0, kCalloc = 0x89350de0, kFree = 0x89350e40;   // [FW]
constexpr uint32_t kRealloc = 0x89350e90, kLargest = 0x89351090, kCompact = 0x89351120;                 // [FW]
constexpr uint32_t kAllocObject = 0x89351690, kLargestObject = 0x893516c0, kFreeObject = 0x893516e0;   // [FW]
constexpr uint32_t kReallocObject = 0x893517c0;                                                         // [FW]
constexpr uint32_t kOut = 0x8C030100;   // the check's RAM: the largest's answer
constexpr uint32_t kMemoryAt = kVc2PoolMemoryAt, kMemoryMost = kVc2PoolMemoryMost, kVtables = kVc2PoolVtables;
constexpr uint32_t kVtableBytes = kVc2PoolVtableBytes, kVtableAt = 0x14, kHeader = 24, kUnitsAt = 0xc;
constexpr int kSteps = 160;
using Logs = Vc2PoolLogs;
using PortHost = Vc2PoolCheckHost;
void hookRom(Vc2Fw& fw, Logs& logs) { vc2HookPoolRom(fw, logs); }

// A live block the check holds: its address as its call gave it, and whether that is an object (its block) or a payload
struct Live {
    uint32_t at;
    bool object;
};

// A case's two sides and what it draws from
struct Sides {
    Vc2Fw& fw;
    Vc2WavePool& pool;
    PortHost& host;
    std::vector<Live>& live;
    std::mt19937& rng;
    bool uniform = false;   // a case asking for one size nearly always, so freed blocks tie for the largest
};

// One drawn call on both sides: what rom.4 gave, the port's, the block to dress (0 for none) and whether its payload
// takes noise (calloc's stays zeroed)
struct Call {
    uint32_t theirs = 0, mine = 0, block = 0;
    bool ok = true, noisy = true;
};

Call rom(Vc2Fw& fw, uint32_t f, std::initializer_list<uint32_t> args) {
    Call c;
    c.ok = fw.cpu.call(f, args, kVc2Sp, &c.theirs, kVc2LoadSteps);
    return c;
}

uint32_t drawBytes(Sides& s) { return s.uniform && s.rng() % 8 ? 1000 : vc2PoolDrawBytes(s.rng, s.pool); }

// A block both sides gave: held, and dressed, its payload's noise unless `noisy` is false (calloc's stays zeroed)
Call gave(Sides& s, Call c, bool object, bool noisy) {
    if (c.mine == 0 || c.mine != c.theirs) return c;
    s.live.push_back({ c.mine, object });
    c.block = object ? c.mine : c.mine - kHeader;
    c.noisy = noisy;
    return c;
}

Call allocCall(Sides& s) {
    const uint32_t bytes = drawBytes(s);
    Call c = rom(s.fw, kAlloc, { Vc2WavePool::kAt, bytes });
    c.mine = s.pool.alloc(bytes);
    return gave(s, c, false, true);
}

Call allocObjectCall(Sides& s) {
    const uint32_t bytes = drawBytes(s), extra = s.rng() % 41;
    Call c = rom(s.fw, kAllocObject, { bytes, extra });
    c.mine = s.pool.allocObject(bytes, extra);
    return gave(s, c, true, true);
}

Call callocCall(Sides& s) {
    const uint32_t n = 1 + s.rng() % 8, size = 1 + drawBytes(s) / 8;
    Call c = rom(s.fw, kCalloc, { Vc2WavePool::kAt, n, size });
    c.mine = s.pool.calloc(n, size);
    return gave(s, c, false, false);
}

Call largestCall(Sides& s) {
    const bool object = s.rng() % 2;
    s.fw.cpu.wr32(kOut, 0);
    Call c = object ? rom(s.fw, kLargestObject, { 0, kOut }) : rom(s.fw, kLargest, { Vc2WavePool::kAt, kOut });
    uint32_t size = 0;
    c.mine = object ? s.pool.largestObject(size) : s.pool.largest(size);
    if (c.ok && c.theirs && s.fw.cpu.rd32(kOut) != size) c.mine = ~c.mine;   // the size it said differs
    return gave(s, c, object, true);
}

// A free, now and then of a null pointer (as a payload or an object)
Call freeCall(Sides& s) {
    if (s.live.empty() || s.rng() % 16 == 0) {
        const bool object = s.rng() % 2;
        const Call c = object ? rom(s.fw, kFreeObject, { 0 }) : rom(s.fw, kFree, { Vc2WavePool::kAt, 0 });
        object ? s.pool.freeObject(0) : s.pool.free(0);
        return { 0, 0, 0, c.ok };
    }
    const size_t k = s.rng() % s.live.size();
    const Live held = s.live[k];
    s.live.erase(s.live.begin() + static_cast<long>(k));
    const Call c = held.object ? rom(s.fw, kFreeObject, { held.at }) : rom(s.fw, kFree, { Vc2WavePool::kAt, held.at });
    held.object ? s.pool.freeObject(held.at) : s.pool.free(held.at);
    return { 0, 0, 0, c.ok };
}

// A realloc, now and then of a null payload (an object's null rom.4 reads at 24 bytes in: not drawn)
Call reallocCall(Sides& s) {
    if (s.live.empty() || s.rng() % 16 == 0) {
        const uint32_t bytes = drawBytes(s);
        Call c = rom(s.fw, kRealloc, { Vc2WavePool::kAt, 0, bytes });
        c.mine = s.pool.realloc(0, bytes, s.host);
        return c;
    }
    Live& held = s.rng() % 2 ? s.live.back() : s.live[s.rng() % s.live.size()];   // the newest half the time
    const uint32_t extra = held.object ? s.rng() % 41 : 0, block = held.object ? held.at : held.at - kHeader;
    uint32_t bytes = s.rng() % 4 == 0 ? vc2PoolFittingBytes(s.rng, s.pool, block) : drawBytes(s);
    if (bytes >= extra && s.rng() % 2) bytes -= extra;   // an object's fit takes its extra as part of the size
    // A realloc to the same size can run rom.4 into its stop on a broken ring (Vc2WavePool.h): a size up instead
    if (Vc2WavePool::unitsFor(bytes + extra) == s.pool.word(block + kUnitsAt)) bytes += 40;
    Call c = held.object ? rom(s.fw, kReallocObject, { held.at, bytes, extra })
                         : rom(s.fw, kRealloc, { Vc2WavePool::kAt, held.at, bytes });
    c.mine = held.object ? s.pool.reallocObject(held.at, bytes, extra, s.host) : s.pool.realloc(held.at, bytes, s.host);
    if (c.mine) held.at = c.mine;
    return c;
}

Call compactCall(Sides& s) {
    const Call c = rom(s.fw, kCompact, { Vc2WavePool::kAt });
    s.pool.compact(s.host);
    s.live.clear();   // the blocks moved: what the check held is gone
    return { 0, 0, 0, c.ok };
}

Call setUpCall(Sides& s) {
    const uint32_t bytes = vc2PoolDrawSize(s.rng);
    Call c = rom(s.fw, kSetUp, { Vc2WavePool::kAt, bytes });
    c.mine = s.pool.setUp(bytes, s.host);
    s.live.clear();
    return c;
}

// The drawn calls by their weights out of 100
struct Drawn {
    uint32_t weight;
    Call (*call)(Sides&);
    const char* name;
};
constexpr Drawn kDrawn[] = { { 18, allocCall, "alloc" },     { 16, allocObjectCall, "an object's alloc" },
                             { 5, callocCall, "calloc" },    { 8, largestCall, "the largest" },
                             { 25, freeCall, "free" },       { 15, reallocCall, "realloc" },
                             { 12, compactCall, "compact" }, { 1, setUpCall, "set-up" } };

// On a pool the OS gave no memory, the calls rom.4 answers without reading a block: the allocs and a null's free
constexpr Drawn kBare[] = { { 40, allocCall, "alloc" }, { 30, allocObjectCall, "an object's alloc" },
                            { 15, callocCall, "calloc" }, { 15, freeCall, "free" } };
constexpr int kBareSteps = 4;
// A uniform case's: the pool fills and holes of one size build up, which the largest then has to choose among
constexpr Drawn kUniform[] = { { 40, allocCall, "alloc" }, { 15, largestCall, "the largest" },
                               { 40, freeCall, "free" },   { 5, reallocCall, "realloc" } };

// A drawn call by the weights of the table for the pool (with memory or none, a uniform case's), and its name
std::pair<Call, const char*> drawnCall(Sides& s) {
    const std::span<const Drawn> withMemory[] = { kDrawn, kUniform };
    uint32_t pick = s.rng() % 100;
    for (const Drawn& d : s.pool.words.base == 0 ? std::span<const Drawn>(kBare) : withMemory[s.uniform]) {
        if (pick < d.weight) return { d.call(s), d.name };
        pick -= d.weight;
    }
    return { {}, "" };
}

std::string poolDifference(Vc2Fw& fw, const Vc2WavePool& pool, const Logs& theirs, const Logs& mine) {
    Sh4& c = fw.cpu;
    if (std::string d = vc2BytesDifference(c, Vc2WavePool::kAt, &pool.words, sizeof pool.words, "the pool's words");
        !d.empty())
        return d;
    if (!pool.memory.empty())
        if (std::string d = vc2BytesDifference(c, pool.words.base, pool.memory.data(), pool.memory.size(), "memory");
            !d.empty())
            return d;
    if (mine.moved != theirs.moved) return "the moved objects' calls";
    return mine.os == theirs.os ? "" : "the OS's calls";
}

// A block given on both sides: noise for its payload when `noisy`, and a vtable at +0x14, which the pool calls when
// it moves any used block
void dress(Sides& s, uint32_t block, bool noisy) {
    const uint32_t units = noisy ? s.pool.word(block + kUnitsAt) : 0;
    for (uint32_t i = 0; i < units * kHeader; ++i) {
        const auto b = static_cast<uint8_t>(s.rng());
        if (uint8_t* mine = s.pool.bytesAt(block + kHeader + i)) *mine = b;
        s.fw.cpu.wr8(block + kHeader + i, b);
    }
    const uint32_t vtable = kVtables + kVtableBytes * (s.rng() % 2);
    s.pool.setWord(block + kVtableAt, vtable);
    s.fw.cpu.wr32(block + kVtableAt, vtable);
}
} // namespace

long vc2WavePoolPart(Vc2Fw& fw, const Vc2Run& run) {
    Vc2WavesTally tally { "pool" };
    std::mt19937 rng(static_cast<uint32_t>(run.seed) ^ 0x501u);
    fw.cpu.mapRam(kMemoryAt, kMemoryMost + 0x1000);
    long calls = 0;
    for (int c = 0; c < run.cases && !tally.stopped; ++c) {
        Vc2WavePool pool;
        PortHost host(pool);
        Logs theirs;
        std::vector<Live> live;
        Sides s { fw, pool, host, live, rng, rng() % 4 == 0 };
        hookRom(fw, theirs);
        fw.cpu.write(Vc2WavePool::kAt, &pool.words, sizeof pool.words);
        const uint32_t most = s.uniform ? 8192 : kMemoryMost;   // a uniform case's pool fills, so its holes tie
        const uint32_t bytes = c % 8 == 7 ? vc2PoolDrawSize(rng) : 2048 + rng() % (most - 2048 + 1);
        const Call first = rom(fw, kSetUp, { Vc2WavePool::kAt, bytes });
        std::string d = !first.ok                                   ? "the set-up faults: " + fw.cpu.error()
                        : pool.setUp(bytes, host) != first.theirs ? "the set-up's answer"
                                                                    : poolDifference(fw, pool, theirs, host.logs);
        int bare = 0;   // calls on a pool with no memory, a few at most
        for (int k = 0; k < kSteps && d.empty() && (pool.words.base != 0 || bare++ < kBareSteps); ++k, ++calls) {
            const auto [call, name] = drawnCall(s);
            d = vc2CallOutcome(fw.cpu, call.ok, { call.mine, call.theirs },
                               [&] { return poolDifference(fw, pool, theirs, host.logs); });
            if (d.empty() && call.block) dress(s, call.block, call.noisy);
            if (!d.empty()) d = "call " + std::to_string(k) + ", " + name + ": " + d;
        }
        tally.outcome(d, "case " + std::to_string(c));
    }
    return tally.say("cases of up to " + std::to_string(kSteps) + " calls on a pool (" + std::to_string(calls)
                     + " calls)");
}
#endif
