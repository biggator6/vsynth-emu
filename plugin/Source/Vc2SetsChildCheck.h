#pragma once
// What xt_vc2sets' parts on C's children share (v282 in Vc2SetsCheck.h, its own file from v283): a child's routine on
// both sides, the part's sides and counts, the draws of the parts' words and the Matrix's sums, the readers' hooks, the
// events by routine, a case and a part.  XT_VC2SETS_MAIN only.
#include "Vc2SetsCheck.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <type_traits>

// Where a word of rom.4's notes W is in the port's, the call struct at `at` as the port is given it, and a child's
// routine on both (the pool's words and W written into rom.4's memory, the logs cleared and the answers seeded alike,
// then rom.4's routine and the port's `mine()`; what differs in the pool, W, the calls or the answer)
uint32_t* setsPortWords(SetSides& s, uint32_t at);
Vc2SetCall setsCallAt(SetSides& s, uint32_t at);
std::string setsChildOnBoth(SetSides& s, std::span<const Vc2Region> pools, const Vc2CheckRoutine& routine,
                            std::initializer_list<uint32_t> args, const std::function<uint32_t()>& mine);

// The parts' N words for a child drawn in rom.4's memory by `draw`: every kind's of the 16 parts' first 4 (all), or now
// and then a kind's (v283; any N, v284)
template <size_t N>
using SetWordsDraw = std::array<uint16_t, N> (*)(std::mt19937&);

template <size_t N>
void setsPartWordsDrawn(Vc2Fw& fw, std::mt19937& rng, bool all, const SetPartTable& t, SetWordsDraw<N> draw) {
    constexpr uint32_t kParts = 16, kKinds = 4;
    for (uint32_t p = 0; p < kParts; ++p)
        for (uint32_t k = 0; k < kKinds; ++k) {
            if (!all && rng() % 16) continue;
            for (uint32_t i = 0; i < t.perKind; ++i) {
                const std::array<uint16_t, N> words = draw(rng);
                fw.cpu.write(t.at + t.part * p + t.kind * k + sizeof words * i, words.data(), sizeof words);
            }
        }
}
// A value at one of `Edges`, or any; a byte so (v284), a key follow in cents a key so (the envelopes', the TVA's, v285)
template <class T, T... Edges>
T setsEdgeDrawn(std::mt19937& rng) {
    return setsOneOf(std::to_array<T>({ Edges..., (T)rng() }), rng);
}

template <uint8_t... Edges>
uint8_t setsByteDrawn(std::mt19937& rng) {
    return setsEdgeDrawn<uint8_t, Edges...>(rng);
}

inline constexpr auto setsFollowDrawn = setsEdgeDrawn<int16_t, 0, 100, -100, 1200, -1200, 32767, -32768>;

// Any word, the constructor's M where nothing reads through it (v285)
inline uint32_t setsAnyWord(std::mt19937& rng) {
    return (uint32_t)rng();
}

// The part's N words for a child as its object holds them from +6, drawn by `Of` on a whole object (v284)
template <class Words, size_t N, auto Of>
std::array<uint16_t, N> setsWordsDrawn(std::mt19937& rng) {
    constexpr size_t kPartWordsAt = 6;   // [FW] where every child but the common holds them
    Words w;
    Of(w, rng);
    std::array<uint16_t, N> words;
    std::memcpy(words.data(), reinterpret_cast<const uint8_t*>(&w) + kPartWordsAt, sizeof words);
    return words;
}

// The Matrix's sums the readers answer drawn (all, or now and then each)
void setsSumsDrawn(Vc2Fw& fw, std::mt19937& rng, bool all);
// The Matrix group's `readers` hooked to log the M they're given and answer the sums
void setsHookReaders(SetSides& s, std::span<const uint32_t> readers);

// A child's draws before an event (all at a case's first): the sets' part's, the part's N words by `Table` as `Of` draws
// an object's, the Matrix's sums when it reads them, and now and then each of the pool's objects by `Of` (v284)
template <class Sides, SetPartTable Table, size_t N, auto Of, bool Sums>
void setsChildDraw(Sides& x, std::mt19937& rng, bool all) {
    using Words = std::remove_cvref_t<decltype(x.port.items[0])>;
    setsDraw(x.s, rng, all);
    setsPartWordsDrawn(x.s.fw, rng, all, Table, setsWordsDrawn<Words, N, Of>);
    if constexpr (Sums) setsSumsDrawn(x.s.fw, rng, all);
    for (Words& w : x.port.items)
        if (all || rng() % 8 == 0) Of(w, rng);
}

// A child's part's counts: its events, the calls they made and each kind of event's; a part's own adds its pool's name,
// kPool (v283)
template <size_t Kinds>
struct SetChildCounts {
    long events = 0, calls = 0;
    std::array<long, Kinds> byEvent {};
};

// A child's part's sides (v282): the sets' sides, the port's pool of the child, its counts
template <class Pool, class Counts>
struct SetChildSides {
    explicit SetChildSides(SetSides& sides) : s(sides) {}
    SetSides& s;
    Pool port {};
    Counts n {};
    // One of the pool's objects
    uint32_t any(std::mt19937& rng) const { return Pool::kAt + Pool::kBytes * (uint32_t)(rng() % Pool::kCount); }
    // The pool as rom.4 holds it, said as `what`, and a word of rom.4's the pool keeps (the TVA's step, v285) or none
    std::array<Vc2Region, 2> region(const char* what) const {
        const Vc2Region pool { Pool::kAt, &port.items, sizeof port.items, what };
        if constexpr (requires { Pool::kStepAt; })
            return { pool, Vc2Region { Pool::kStepAt, &port.step, sizeof port.step, "the step" } };
        else
            return { pool, Vc2Region { 0, nullptr, 0, "" } };
    }
};

// A child's routine on an object and C's call (an update): rom.4's on (object, the call struct), the port's
// `mine(object, call)`
template <class Sides, class Mine>
std::string setsChildUpdate(Sides& x, std::mt19937& rng, const Vc2CheckRoutine& r, const char* pool, Mine mine) {
    const uint32_t at = setsCall(x.s, rng), object = x.any(rng);
    const Vc2SetCall call = setsCallAt(x.s, at);
    return setsChildOnBoth(x.s, x.region(pool), r, { object, at }, [&] {
        mine(object, call);
        return 0u;
    });
}

// A child's routine on an object, C's call and a block of W that `Block` draws (a conversion): rom.4's on (object, the
// call struct, block), the port's `mine(object, call, the port's block)`
template <class Sides, auto Block, class Mine>
std::string setsChildConvert(Sides& x, std::mt19937& rng, const Vc2CheckRoutine& r, const char* pool, Mine mine) {
    const uint32_t at = setsCall(x.s, rng), object = x.any(rng), block = Block(rng);
    const Vc2SetCall call = setsCallAt(x.s, at);
    return setsChildOnBoth(x.s, x.region(pool), r, { object, at, block }, [&] {
        mine(object, call, setsPortWords(x.s, block));
        return 0u;
    });
}

// A child's routine on an object and a block of W that `Block` draws (a copy): rom.4's on (object, block), the port's
// `mine(object, the port's block)`
template <class Sides, auto Block, class Mine>
std::string setsChildCopy(Sides& x, std::mt19937& rng, const Vc2CheckRoutine& r, const char* pool, Mine mine) {
    const uint32_t object = x.any(rng), block = Block(rng);
    return setsChildOnBoth(x.s, x.region(pool), r, { object, block }, [&] {
        mine(object, setsPortWords(x.s, block));
        return 0u;
    });
}

// A child's end, a bare return in rom.4: rom.4's on any of the pool's objects and 3, the port's nothing
template <class Sides>
std::string setsChildEnd(Sides& x, std::mt19937& rng, const Vc2CheckRoutine& r, const char* pool) {
    const uint32_t object = x.any(rng);
    return setsChildOnBoth(x.s, x.region(pool), r, { object, 3 }, [] { return 0u; });
}

// A child's events by the port's member and rom.4's routine, each counted in `byEvent[Counted]`, its pool said as its
// counts' kPool: the placement (rom.4's given the object's bytes, a voice's number and an index), the constructor (on
// a block `Block` draws, the kind and the index under `Indices` stacked), the end (a bare return in rom.4), the update
// (an object and C's call), the conversion (and a block `Block` draws) and the copy (an object and a block; the port's
// member given the reads when it takes them)
template <class Sides, uint32_t Routine, uint32_t Bytes, size_t Counted>
std::string setsPlaceEvent(Sides& x, std::mt19937& rng) {
    const uint32_t n = rng() % 256, i = rng() % 16;
    ++x.n.byEvent[Counted];
    const Vc2CheckRoutine r { Routine, "the placement", vc2R0 };
    return setsChildOnBoth(x.s, x.region(decltype(x.n)::kPool), r, { Bytes, n, i },
                           [&] { return x.port.placeOf(n, i); });
}

template <class Sides, auto Block, uint32_t Routine, size_t Counted, uint32_t Indices, auto Matrix = setsAnyWord>
std::string setsMakeEvent(Sides& x, std::mt19937& rng) {
    const uint32_t at = setsCall(x.s, rng);
    const Vc2SetChildCall c { x.any(rng), setsCallAt(x.s, at), Block(rng), Matrix(rng),
                              rng() % 8 ? rng() % 4 : (uint32_t)rng(), rng() % 8 ? rng() % Indices : rng() % 16 };
    x.s.fw.cpu.wr32(kVc2Sp, c.kind);
    x.s.fw.cpu.wr32(kVc2Sp + 4, c.index);
    ++x.n.byEvent[Counted];
    const Vc2CheckRoutine r { Routine, "the constructor", vc2R0 };
    return setsChildOnBoth(x.s, x.region(decltype(x.n)::kPool), r, { c.object, at, c.noteAt, c.matrix }, [&] {
        x.port.construct(c, setsPortWords(x.s, c.noteAt), x.s.parts);
        return c.object;
    });
}

template <class Sides, uint32_t Routine, size_t Counted>
std::string setsEndEvent(Sides& x, std::mt19937& rng) {
    ++x.n.byEvent[Counted];
    return setsChildEnd(x, rng, { Routine, "the end" }, decltype(x.n)::kPool);
}

template <class Sides, auto Member, uint32_t Routine, size_t Counted>
std::string setsUpdateEvent(Sides& x, std::mt19937& rng) {
    ++x.n.byEvent[Counted];
    const auto mine = [&](uint32_t object, const Vc2SetCall& call) { (x.port.*Member)(object, call, x.s.parts); };
    return setsChildUpdate(x, rng, { Routine, "the update" }, decltype(x.n)::kPool, mine);
}

template <class Sides, auto Member, auto Block, uint32_t Routine, size_t Counted>
std::string setsConvertEvent(Sides& x, std::mt19937& rng) {
    ++x.n.byEvent[Counted];
    const auto mine = [&](uint32_t object, const Vc2SetCall& call, uint32_t* block) {
        (x.port.*Member)(object, call, block, x.s.parts);
    };
    return setsChildConvert<Sides, Block>(x, rng, { Routine, "the conversion" }, decltype(x.n)::kPool, mine);
}

template <class Sides, auto Member, auto Block, uint32_t Routine, size_t Counted>
std::string setsCopyEvent(Sides& x, std::mt19937& rng) {
    ++x.n.byEvent[Counted];
    const auto mine = [&](uint32_t object, uint32_t* block) {
        if constexpr (std::is_invocable_v<decltype(Member), decltype(x.port)&, uint32_t, uint32_t*, Vc2SetReads&>)
            (x.port.*Member)(object, block, x.s.parts);
        else
            (x.port.*Member)(object, block);
    };
    return setsChildCopy<Sides, Block>(x, rng, { Routine, "the copy" }, decltype(x.n)::kPool, mine);
}

// A part's counts said by `format`, each %ld in turn the events, each kind of event's and the calls (v284)
void setsSay(const char* format, std::span<const long> values);

template <size_t Kinds>
void setsSayCounts(const char* format, const SetChildCounts<Kinds>& n) {
    std::array<long, Kinds + 2> values {};
    values[0] = n.events;
    std::copy(n.byEvent.begin(), n.byEvent.end(), values.begin() + 1);
    values[Kinds + 1] = n.calls;
    setsSay(format, values);
}

// A child's part's case: noise in the pool, then 400 of `Events`, each after `Draw` (all at the first), its calls
// counted; what differs, or nothing
template <auto Draw, const auto& Events, class Sides>
std::string setsChildCase(Sides& x, std::mt19937& rng) {
    setsNoise(x.port.items, rng);
    const std::span events(Events);
    std::string what;
    for (int k = 0; k < 400 && what.empty(); ++k, ++x.n.events) {
        Draw(x, rng, k == 0);
        what = events[rng() % events.size()](x, rng);
        x.n.calls += (long)x.s.mine.size();
    }
    return what;
}

// A child's part on the seed `offset` after the run's: the sets' part's hooks on its routines taken off, the Matrix
// group's `readers` it reaches hooked, its sides, its cases of `Events`, its counts said by their kSaid; how many of
// its cases differ
template <class Sides, auto Draw, const auto& Events>
long setsChildPart(SetSides& s, const Vc2Run& run, uint32_t offset, std::span<const uint32_t> routines,
                   std::span<const uint32_t> readers = {}) {
    for (const uint32_t at : routines) s.fw.cpu.unhook(at);
    setsHookReaders(s, readers);
    const auto x = std::make_unique<Sides>(s);
    const long different = vc2CheckPart(run, offset, { "XT_VC2SETS_FIRST", "XT_VC2SETS_STOP" }, x->n.events,
                                        [&](std::mt19937& rng) { return setsChildCase<Draw, Events>(*x, rng); });
    setsSayCounts(decltype(x->n)::kSaid, x->n);
    return different;
}
