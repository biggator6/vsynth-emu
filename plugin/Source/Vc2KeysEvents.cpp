// xt_vc2keys' events (v276): each of the keys' routines on both sides, rom.4's then the port's, by rows of the routines
// of a table, of a bitmap, of a glide and of the whole; what differs after, the span and the answer.  XT_VC2KEYS_MAIN
// only.
#ifdef XT_VC2KEYS_MAIN
#include "Vc2KeysCheck.h"
#include "Vc2KeysTables.h"
#include <bit>

namespace {
using K = Vc2Keys;
constexpr uint32_t kHeldAt = 0x0c, kLatchedAt = 0x120;   // [FW] a table's two bitmaps
constexpr size_t kBitsFrom = 8, kGlidesFrom = 13, kAllFrom = 17;

// A key: the one whose counts were drawn last half the time
uint32_t keysKey(KeysSides& s, std::mt19937& rng) { return rng() % 2 ? s.drawnKey : rng() % 128; }
uint32_t keysSource(KeysSides&, std::mt19937& rng) { return rng() % K::kSources; }
uint32_t keysChannel(KeysSides&, std::mt19937& rng) { return rng() % K::kChannels; }
uint32_t keysWord(KeysSides&, std::mt19937& rng) {
    const uint32_t picks[4] = { rng() % 128, rng() % 256, (uint32_t)rng(), rng() % 2 };
    return picks[rng() % 4];
}
// A pedal's value: either side of 64, the byte's top, or any word
uint32_t keysPedal(KeysSides&, std::mt19937& rng) {
    const uint32_t picks[6] = { 0, 63, 64, 127, (uint32_t)rng(), rng() % 128 };
    return picks[rng() % 6];
}

// A routine of a table's on two drawn words (none given, 0), the port's on the same; what it counts, by the table
// before and after and the first word
struct KeysOnTable {
    Vc2CheckRoutine routine;
    uint32_t (*a)(KeysSides&, std::mt19937&);
    uint32_t (*b)(KeysSides&, std::mt19937&);
    uint32_t (*mine)(Vc2KeyTable&, uint32_t, uint32_t);
    long KeysCounts::*counted;
    bool (*shows)(const Vc2KeyTable&, const Vc2KeyTable&, uint32_t);
};

// [FW] the routines of a table
const KeysOnTable kOnTable[] = {
    { { 0x893920c0, "the init" }, keysSource, keysChannel,
      [](Vc2KeyTable& t, uint32_t a, uint32_t b) { t.init(a, b); return 0u; }, nullptr, nullptr },
    { { 0x89392040, "the reset" }, nullptr, nullptr, [](Vc2KeyTable& t, uint32_t, uint32_t) { t.reset(); return 0u; },
      nullptr, nullptr },
    { { 0x89391e90, "the note-on" }, keysKey, keysKey,
      [](Vc2KeyTable& t, uint32_t a, uint32_t b) { t.noteOn(a, b); return 0u; }, &KeysCounts::saturated,
      [](const Vc2KeyTable& before, const Vc2KeyTable&, uint32_t a) { return before.ons[a] == 0xff; } },
    { { 0x89391ef0, "the note-off" }, keysKey, keysWord,
      [](Vc2KeyTable& t, uint32_t a, uint32_t b) { t.noteOff(a, b); return 0u; }, &KeysCounts::released,
      [](const Vc2KeyTable& before, const Vc2KeyTable& t, uint32_t) { return before.held != t.held; } },
    { { 0x89391f60, "the Hold" }, keysPedal, nullptr,
      [](Vc2KeyTable& t, uint32_t a, uint32_t) { t.setHold(a); return 0u; }, nullptr, nullptr },
    { { 0x89391f70, "the Sostenuto" }, keysPedal, nullptr,
      [](Vc2KeyTable& t, uint32_t a, uint32_t) { t.setSostenuto((int32_t)a); return 0u; }, &KeysCounts::latched,
      [](const Vc2KeyTable& before, const Vc2KeyTable&, uint32_t a) { return ((before.sostenuto ^ a) & 0x40) != 0; } },
    { { 0x89391fe0, "the clear" }, nullptr, nullptr, [](Vc2KeyTable& t, uint32_t, uint32_t) { t.clear(); return 0u; },
      nullptr, nullptr },
    { { 0x89392280, "the velocity factor", vc2Fr0 }, keysKey, nullptr,
      [](Vc2KeyTable& t, uint32_t a, uint32_t) { return std::bit_cast<uint32_t>(t.velocityFactor(a)); }, nullptr,
      nullptr },
};

template <size_t I>
std::string keysTableEvent(KeysSides& s, std::mt19937& rng) {
    const KeysOnTable& e = kOnTable[I];
    const uint32_t at = rng() % 2 ? s.drawnTable : keysAnyTable(rng);
    const uint32_t a = e.a ? e.a(s, rng) : 0, b = e.b ? e.b(s, rng) : 0;
    Vc2KeyTable& t = s.port.table(at);
    const Vc2KeyTable before = t;
    ++s.n.byEvent[I];
    const std::string what = keysOnBoth(s, e.routine, { at, a, b }, [&] { return e.mine(t, a, b); });
    if (e.counted) s.n.*e.counted += e.shows(before, t, a);
    return what;
}

// A routine of a bitmap's on a range drawn (0…127 each), the port's on the same
struct KeysOnBits {
    Vc2CheckRoutine routine;
    uint32_t (*mine)(Vc2KeyBits&, int32_t, int32_t);
};

// [FW] the held keys' queries
const KeysOnBits kOnBits[] = {
    { { 0x89392670, "the highest in a range", vc2R0 },
      [](Vc2KeyBits& b, int32_t lo, int32_t hi) { return (uint32_t)vc2KeysHighestIn(b, lo, hi); } },
    { { 0x893925e0, "the highest", vc2R0 },
      [](Vc2KeyBits& b, int32_t, int32_t) { return (uint32_t)vc2KeysHighest(b); } },
    { { 0x89392630, "the range's set" }, [](Vc2KeyBits& b, int32_t lo, int32_t hi) {
          vc2KeysSet(b, lo, hi);
          return 0u;
      } },
    { { 0x893925a0, "the fill" }, [](Vc2KeyBits& b, int32_t, int32_t) {
          vc2KeysFill(b);
          return 0u;
      } },
    { { 0x893925b0, "the test", vc2R0 }, [](Vc2KeyBits& b, int32_t, int32_t) { return (uint32_t)vc2KeysAny(b); } },
};

// A range: anywhere, or ending in its first's word, or a key alone
std::array<int32_t, 2> keysRange(std::mt19937& rng) {
    const int32_t lo = (int32_t)(rng() % 128);
    const int32_t near = std::min(127, lo + (int32_t)(rng() % 16));
    const int32_t picks[3] = { (int32_t)(rng() % 128), near, lo };
    return { lo, picks[rng() % 3] };
}

template <size_t I>
std::string keysBitsEvent(KeysSides& s, std::mt19937& rng) {
    const KeysOnBits& e = kOnBits[I];
    const uint32_t table = keysAnyTable(rng);
    const bool latched = rng() % 4 == 0;
    Vc2KeyBits& bits = latched ? s.port.table(table).latched : s.port.table(table).held;
    const auto [lo, hi] = keysRange(rng);
    ++s.n.byEvent[kBitsFrom + I];
    uint32_t answer = 0;
    const uint32_t at = table + (latched ? kLatchedAt : kHeldAt);
    const std::string what = keysOnBoth(s, e.routine, { at, (uint32_t)lo, (uint32_t)hi }, [&] {
        answer = e.mine(bits, lo, hi);
        return answer;
    });
    s.n.found += I == 0 && answer != 0xffffffffu;
    return what;
}

// A routine of a glide's, the port's on the same at the clock drawn
struct KeysOnGlide {
    Vc2CheckRoutine routine;
    uint32_t (*mine)(Vc2Keys&, uint32_t, int32_t);
};

// [FW] the routines of a glide; the note-off is given the range every caller gives
const KeysOnGlide kOnGlide[] = {
    { { 0x893922b0, "the glide's constructor", vc2R0 }, [](Vc2Keys& k, uint32_t at, int32_t) {
          k.glide(at).construct(at);
          return at;
      } },
    { { 0x893923c0, "the glide's reset" }, [](Vc2Keys& k, uint32_t at, int32_t) {
          k.glide(at).reset();
          return 0u;
      } },
    { { 0x893922d0, "the glide's note-on" }, [](Vc2Keys& k, uint32_t at, int32_t ms) {
          k.glide(at).noteOn(ms, k.table(k.glide(at).table));
          return 0u;
      } },
    { { 0x89392360, "the glide's note-off", vc2R0 }, [](Vc2Keys& k, uint32_t at, int32_t) {
          return (uint32_t)k.glide(at).noteOff(k.table(k.glide(at).table));
      } },
};

// The clock at a note-on: soon after the last, a while after, long after, or before it
int32_t keysMs(const Vc2KeyGlide& g, std::mt19937& rng) {
    const uint32_t back = (uint32_t)kKeysClockMost - rng() % 64;
    const uint32_t steps[4] = { rng() % 40, rng() % 70000, rng() % (1u << 29), back };
    return (int32_t)(((uint32_t)g.lastMs + steps[rng() % 4]) % (uint32_t)kKeysClockMost);
}

template <size_t I>
std::string keysGlideEvent(KeysSides& s, std::mt19937& rng) {
    const KeysOnGlide& e = kOnGlide[I];
    const uint32_t at = keysAnyGlide(rng);
    Vc2KeyGlide& g = s.port.glide(at);
    if (rng() % 2) g.key = s.port.table(g.table).offKey;
    s.ms = keysMs(g, rng);
    ++s.n.byEvent[kGlidesFrom + I];
    uint32_t answer = 0;
    const std::string what = keysOnBoth(s, e.routine, { at, 0, 127 }, [&] {
        answer = e.mine(s.port, at, s.ms);
        return answer;
    });
    s.n.glidesFound += I == 3 && answer != 0xffffffffu;
    return what;
}

// [FW] the initializers
const std::pair<Vc2CheckRoutine, void (Vc2Keys::*)()> kOnAll[] = {
    { { 0x89392190, "the tables' initializer" }, &K::staticInit },
    { { 0x89392410, "the glides' initializer" }, &K::glidesInit },
    { { 0x89392560, "the glides' constructor" }, &K::constructGlides },
};

template <size_t I>
std::string keysAllEvent(KeysSides& s, std::mt19937&) {
    ++s.n.byEvent[kAllFrom + I];
    return keysOnBoth(s, kOnAll[I].first, {}, [&] {
        (s.port.*kOnAll[I].second)();
        return 0u;
    });
}

const KeysEvent kEvents[] = {
    keysTableEvent<0>, keysTableEvent<1>, keysTableEvent<2>, keysTableEvent<2>, keysTableEvent<3>, keysTableEvent<3>,
    keysTableEvent<4>, keysTableEvent<5>, keysTableEvent<5>, keysTableEvent<6>, keysTableEvent<7>, keysBitsEvent<0>,
    keysBitsEvent<0>, keysBitsEvent<1>, keysBitsEvent<2>, keysBitsEvent<3>, keysBitsEvent<4>, keysGlideEvent<0>,
    keysGlideEvent<1>, keysGlideEvent<2>, keysGlideEvent<2>, keysGlideEvent<3>, keysGlideEvent<3>,
};
// The initializers, each setting every table or glide up
const KeysEvent kInits[] = { keysAllEvent<0>, keysAllEvent<1>, keysAllEvent<2> };
} // namespace

std::span<const KeysEvent> keysEvents() { return kEvents; }
std::span<const KeysEvent> keysInits() { return kInits; }
#endif
