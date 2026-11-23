// xt_vc2waves' records part (v502): rom.4's record making 0x89353b40, chunk making 0x89353ef0 and deletion 0x89354490
// on its store at 0x8997c52c and pool at 0x8997c51c against Vc2WaveRecords, call by call.  Each case sets a small pool
// up (8 to 64 KB, rom.4's 0x89350c10 and the port's) under a store with no records and its key −1, then up to 120
// drawn calls: a record made (a key of twelve, three to a list, and a type 0…40), a chunk made (its number 1…12 mostly,
// now and then 0, 9 or 13; its extra bytes mostly under 100, now and then more than the pool holds), a record
// deleted, the pool compacted (each moved object told through its class's method: rom.4's own, the port's by its
// vtable), an object of the check's own alloc'd or freed (its vtable logs its moves on both sides, its payload noise),
// and the store's key set (−1 mostly) on both sides.  After each call: the answer, the store's words, the pool's words
// and whole memory, and both sides' logs (the OS's calls, the check's objects' moves).  XT_VC2WAVES_MAIN only.
#ifdef XT_VC2WAVES_MAIN
#include "Vc2WaveRecords.h"
#include "Vc2WavesCheck.h"

namespace {
constexpr uint32_t kMake = 0x89353b40, kMakeChunk = 0x89353ef0, kRemove = 0x89354490;   // [FW]
constexpr uint32_t kPoolSetUp = 0x89350c10, kCompact = 0x89351120;                      // [FW]
constexpr uint32_t kAllocObject = 0x89351690, kFreeObject = 0x893516e0;                // [FW]
constexpr uint32_t kStoreAt = Vc2WaveStore::kAt, kPoolAt = Vc2WavePool::kAt, kVtableAt = 0x14, kHeader = 24;
constexpr int32_t kKeys[] = { 0, 128, 0x1000000, 1, 129, 257, 2, 3000, 3128, 3001, 5000, 77 };   // lists 0, 1, 56 shared
constexpr uint32_t kOddNumbers[] = { 0, 9, 13 };
constexpr int kSteps = 120;

// A case's two sides and what it draws from
struct StoreSides {
    Vc2Fw& fw;
    Vc2WaveStore& store;
    Vc2WavePool& pool;
    Vc2WaveRecords& records;
    std::mt19937& rng;
    std::vector<uint32_t>& objects;   // the check's own objects
};

// One drawn call's answers: rom.4's and the port's, and whether rom.4's ran
struct Answer {
    int32_t theirs = 0, mine = 0;
    bool ok = true;
};

Answer romAnswer(Vc2Fw& fw, uint32_t f, std::initializer_list<uint32_t> args) {
    uint32_t r0 = 0;
    Answer a;
    a.ok = fw.cpu.call(f, args, kVc2Sp, &r0, kVc2LoadSteps);
    a.theirs = static_cast<int32_t>(r0);
    return a;
}

int32_t drawKey(std::mt19937& rng) { return kKeys[rng() % std::size(kKeys)]; }
uint32_t asArg(int32_t v) { return static_cast<uint32_t>(v); }

Answer recordCall(StoreSides& s) {
    const int32_t key = drawKey(s.rng);
    const auto type = static_cast<uint8_t>(s.rng() % 41);
    Answer a = romAnswer(s.fw, kMake, { kStoreAt, asArg(key), type });
    a.mine = s.records.make(key, type);
    return a;
}

Answer chunkCall(StoreSides& s) {
    const int32_t key = drawKey(s.rng);
    const uint32_t n = s.rng() % 8 ? 1 + s.rng() % 12 : kOddNumbers[s.rng() % 3];
    const uint32_t extra = s.rng() % 8 ? s.rng() % 100 : 0x10000 + s.rng() % 0x1000;
    Answer a = romAnswer(s.fw, kMakeChunk, { kStoreAt, asArg(key), n, extra });
    a.mine = s.records.makeChunk(key, n, extra);
    return a;
}

Answer removeCall(StoreSides& s) {
    const int32_t key = drawKey(s.rng);
    Answer a = romAnswer(s.fw, kRemove, { kStoreAt, asArg(key) });
    a.mine = s.records.remove(key);
    return a;
}

Answer storeCompactCall(StoreSides& s) {
    const Answer a = romAnswer(s.fw, kCompact, { kPoolAt });
    s.pool.compact(s.records);
    s.objects.clear();   // the check's objects moved: their addresses are gone
    return { 0, 0, a.ok };
}

// An object of the check's own: alloc'd (its vtable the check's, its payload noise) or, half the time, one freed
Answer objectCall(StoreSides& s) {
    if (!s.objects.empty() && s.rng() % 2) {
        const size_t k = s.rng() % s.objects.size();
        const uint32_t object = s.objects[k];
        s.objects.erase(s.objects.begin() + static_cast<long>(k));
        const Answer a = romAnswer(s.fw, kFreeObject, { object });
        s.pool.freeObject(object);
        return { 0, 0, a.ok };
    }
    // a sixth of the time nearly all that's free, so a record or chunk then finds no room
    const uint32_t most = s.pool.words.free > 400 ? s.pool.words.free - 100 - s.rng() % 300 : 1;
    const uint32_t bytes = s.rng() % 6 ? 1 + s.rng() % 300 : most, extra = s.rng() % 21;
    Answer a = romAnswer(s.fw, kAllocObject, { bytes, extra });
    const uint32_t object = s.pool.allocObject(bytes, extra);
    a.mine = static_cast<int32_t>(object);
    if (object == 0 || a.mine != a.theirs) return a;
    s.objects.push_back(object);
    const uint32_t vtable = kVc2PoolVtables + kVc2PoolVtableBytes * (s.rng() % 2);
    s.pool.setWord(object + kVtableAt, vtable);
    s.fw.cpu.wr32(object + kVtableAt, vtable);
    for (uint32_t i = 0; i < bytes; ++i) {
        const auto b = static_cast<uint8_t>(s.rng());
        *s.pool.bytesAt(object + kHeader + i) = b;
        s.fw.cpu.wr8(object + kHeader + i, b);
    }
    return a;
}

// The store's key on both sides: −1 three times in four, else a key a record may have
Answer keyCall(StoreSides& s) {
    s.store.words.key = s.rng() % 4 ? -1 : drawKey(s.rng);
    s.fw.cpu.wr32(kStoreAt, asArg(s.store.words.key));
    return {};
}

struct StoreDraw {
    uint32_t weight;
    Answer (*call)(StoreSides&);
    const char* name;
};
constexpr StoreDraw kStoreDraws[] = { { 22, recordCall, "make" },        { 30, chunkCall, "a chunk" },
                                      { 12, removeCall, "remove" },      { 8, storeCompactCall, "compact" },
                                      { 18, objectCall, "an object" },   { 10, keyCall, "the key" } };

std::pair<Answer, const char*> drawnStoreCall(StoreSides& s) {
    uint32_t pick = s.rng() % 100;
    for (const StoreDraw& d : kStoreDraws) {
        if (pick < d.weight) return { d.call(s), d.name };
        pick -= d.weight;
    }
    return { {}, "" };
}

std::string recordsDifference(Vc2Fw& fw, const StoreSides& s, const Vc2PoolLogs& theirs, const Vc2PoolLogs& mine) {
    const std::tuple<uint32_t, const void*, size_t, const char*> regions[] = {
        { kStoreAt, &s.store.words, sizeof s.store.words, "the store's words" },
        { kPoolAt, &s.pool.words, sizeof s.pool.words, "the pool's words" },
        { s.pool.words.base, s.pool.memory.data(), s.pool.memory.size(), "the pool's memory" },
    };
    for (const auto& [at, ours, bytes, what] : regions)
        if (std::string d = vc2BytesDifference(fw, at, ours, bytes, what); !d.empty()) return d;
    if (mine.moved != theirs.moved) return "the check's objects' moves";
    return mine.os == theirs.os ? "" : "the OS's calls";
}

// A case: the store with no records and its key −1, a small pool set up on both sides, then the drawn calls
std::string storeCase(Vc2Fw& fw, std::mt19937& rng, long& calls) {
    Vc2WaveStore store;
    store.words.key = -1;
    Vc2WavePool pool;
    Vc2PoolCheckHost os(pool);
    Vc2WaveRecords records(store, pool, os);
    std::vector<uint32_t> objects;
    StoreSides s { fw, store, pool, records, rng, objects };
    Vc2PoolLogs theirs;
    vc2HookPoolRom(fw, theirs);
    fw.cpu.write(kStoreAt, &store.words, sizeof store.words);
    fw.cpu.write(kPoolAt, &pool.words, sizeof pool.words);
    const uint32_t bytes = 8192 + rng() % (kVc2PoolMemoryMost - 8192 + 1);
    const Answer setUp = romAnswer(fw, kPoolSetUp, { kPoolAt, bytes });
    pool.setUp(bytes, records);
    const auto memory = [&] { return recordsDifference(fw, s, theirs, os.logs); };
    std::string d = vc2CallOutcome(fw, setUp.ok, { 0, 0 }, memory);
    for (int k = 0; k < kSteps && d.empty(); ++k, ++calls) {
        const auto [a, name] = drawnStoreCall(s);
        d = vc2CallOutcome(fw, a.ok, { a.mine, a.theirs }, memory);
        if (!d.empty()) d = "call " + std::to_string(k) + ", " + name + ": " + d;
    }
    return d;
}
} // namespace

long vc2WaveRecordsPart(Vc2Fw& fw, const Vc2Run& run) {
    Vc2WavesTally tally { "records" };
    std::mt19937 rng(static_cast<uint32_t>(run.seed) ^ 0x502u);
    fw.cpu.mapRam(kVc2PoolMemoryAt, kVc2PoolMemoryMost + 0x1000);   // the check's OS memory, and the overhang
    long calls = 0;
    for (int c = 0; c < run.cases && !tally.stopped; ++c)
        tally.outcome(storeCase(fw, rng, calls), "case " + std::to_string(c));
    return tally.say("cases of up to " + std::to_string(kSteps) + " calls on a store (" + std::to_string(calls)
                     + " calls)");
}
#endif
