#pragma once
// xt_vc2sets' shared parts (v280): the calls each side logs (C's children's routines, each with the arguments rom.4's is
// given, a call struct read through its pointer, the constructor's stacked words), what they answer (an allocator an
// object or none, a refresh a word), drawn by a generator per side from one seed; the port's parts, which read the
// voices, key tables, channels, parts and the part's bytes by kind where the draws wrote them in rom.4's memory; and the
// two sides.  XT_VC2SETS_MAIN only.
#include "Vc2CheckMain.h"
#include "Vc2Sets.h"
#include <span>
#include <vector>

// In the RAM Vc2Fw::start maps: the voices' 16 bytes, the key tables, the channels, the parts and the children
inline constexpr uint32_t kSetsVoicesAt = 0x8C030000, kSetsKeysAt = 0x8C031000, kSetsChannelsAt = 0x8C032000;
inline constexpr uint32_t kSetsPartsAt = 0x8C033000, kSetsChildrenAt = 0x8C034000;
inline constexpr uint32_t kSetsVoiceBytes = 0x60, kSetsKeyBytes = 0x40, kSetsChannelBytes = 0x100;
inline constexpr uint32_t kSetsPartBytes = 0x10, kSetsChildBytes = 0x40;
inline constexpr int kSetsObjects = 4, kSetsChildren = 64;
inline constexpr uint32_t kSetsVoiceWords = 0x34, kSetsByteAt = 0x53;   // [FW] the voice's words a call points at
inline constexpr uint32_t kSetsModesAt = 0x89998a14;                // [FW] the part's bytes by kind
inline constexpr uint32_t kSetsMatrixAt = 0x89997108;               // [FW] the part's own M
inline constexpr uint32_t kSetsControls = 0x84;                     // [FW] a channel's controllers
inline constexpr uint32_t kSetsSettingsAt = 0x89997398;             // [FW] the Matrix's settings G
inline constexpr uint32_t kSetsCommonsAt = 0x89997090, kSetsCommonBytes = 6;   // [FW] the parts' words for the common
// [FW] the parts' words for the envelopes, by part, kind and index; the clock object (v282)
inline constexpr uint32_t kSetsEnvelopesAt = 0x89e2765c, kSetsEnvelopePart = 2080, kSetsEnvelopeKind = 130;
inline constexpr uint32_t kSetsEnvelopeIndex = 10, kSetsClockAt = 0x8995cb40;
inline constexpr uint32_t kSetsSumsAt = 0x8C035000;                  // in Vc2Fw's RAM: the Matrix's sums drawn

// The calls logged: C's children's methods, and a Matrix group's reader (v282, the envelopes' part)
enum SetMethod : int32_t { kSetAllocate, kSetConstruct, kSetEnd, kSetConvert, kSetUpdate, kSetCopy, kSetRelease,
                           kSetRefresh, kSetMatrixSum, kSetMethods };

// A call as either side made it: which method, the routine it reached, its arguments (r4…, a struct's three words where
// it takes one, the stacked words)
struct SetCall {
    int32_t kind;
    uint32_t routine;
    std::array<uint32_t, 8> args;
    bool operator==(const SetCall&) const = default;
};

// The port's parts: rom.4's memory read where the draws wrote it, the children's calls logged and answered
class SetPortParts final : public Vc2SetParts {
public:
    SetPortParts(Vc2Fw& fw, std::mt19937& answers, std::vector<SetCall>& log) : fw_(fw), answers_(answers), log_(log) {}
    uint32_t allocate(const Vc2SetChild& c, uint32_t number, uint32_t index) override;
    void construct(const Vc2SetChild& c, const Vc2SetChildCall& call) override;
    void end(const Vc2SetChild& c, uint32_t object) override;
    void convert(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call, uint32_t noteAt) override;
    void update(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) override;
    void copy(const Vc2SetChild& c, uint32_t object, uint32_t noteAt) override;
    void release(const Vc2SetChild& c, uint32_t object, uint32_t velocity, uint32_t noteAt) override;
    uint32_t refresh(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) override;
    Vc2SetVoice voiceAt(uint32_t channelAt) override;
    uint8_t firstByte(uint32_t object) override { return fw_.cpu.rd8(object); }
    uint32_t channelOf(uint32_t keys) override { return fw_.cpu.rd32(keys); }
    int16_t bendOf(uint32_t channel) override { return static_cast<int16_t>(fw_.cpu.rd16(channel)); }
    uint8_t controlOf(uint32_t channel, uint32_t number) override;
    uint8_t modeOf(uint32_t part, uint32_t kind) override;
    std::array<uint16_t, 3> commonOf(uint32_t part) override;
    std::array<uint16_t, 5> envelopeOf(uint32_t part, uint32_t kind, uint32_t index) override;
    Vc2SetClock clock() override;
    int16_t matrixSum(uint32_t reader, uint32_t matrix, uint32_t offset) override;

private:
    void logged(int32_t kind, uint32_t routine, std::array<uint32_t, 8> args);
    Vc2Fw& fw_;
    std::mt19937& answers_;
    std::vector<SetCall>& log_;
};

// Every byte of `t` drawn
template <class T>
void setsNoise(T& t, std::mt19937& rng) {
    auto* b = reinterpret_cast<uint8_t*>(&t);
    for (std::size_t i = 0; i < sizeof t; ++i) b[i] = (uint8_t)rng();
}

// One of `values`, drawn (any draws in them made first, left to right)
template <class T, size_t N>
T setsOneOf(const std::array<T, N>& values, std::mt19937& rng) {
    return values[rng() % N];
}

// An answer of a hooked call: an allocator's object (none one time in eight), a refresh's word
uint32_t setAnswer(int32_t kind, std::mt19937& rng);

struct SetCounts {
    long events = 0;
    std::array<long, 11> byEvent {};
    std::array<long, kSetMethods> calls {};
    long noCommon = 0;
};

// The two sides: rom.4's interpreter, its hooks and log; the port's sets and M, notes, G, the part's M, answers and log
struct SetSides {
    explicit SetSides(Vc2Fw& rig);
    Vc2Fw& fw;
    Vc2Sets port {};
    Vc2Notes notes {};
    Vc2MatrixSettings g {};
    Vc2MatrixWords partM {};
    Vc2MatrixLinks partLinks {};
    std::mt19937 romAnswers {}, portAnswers {};
    std::vector<SetCall> rom {}, mine {};
    SetPortParts parts;
    SetCounts n {};
    uint32_t seed = 0;   // the answers' for the next event
    Vc2SetMatrixIn in() { return { g, partM, partLinks }; }
};

// rom.4's routine on `args` after the port's words are written into its memory, the logs cleared and the answers seeded
// alike, then the port's `mine()` giving its answer; what differs in the sets, M, the part's M, the notes, the calls or
// the answer
std::string setsOnBoth(SetSides& s, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                       const std::function<uint32_t()>& mine);

using SetEvent = std::string (*)(SetSides&, std::mt19937&);
std::span<const SetEvent> setEvents();
// The draws before an event (all at a case's first): what rom.4 reads besides the port's words, and those words
void setsDraw(SetSides& s, std::mt19937& rng, bool all);
// A set, a note, a call struct (written at kVc2Args, its pointer the answer), a voice number drawn
uint32_t setsSet(std::mt19937& rng);
uint32_t setsNote(std::mt19937& rng);
uint32_t setsCall(SetSides& s, std::mt19937& rng);

// What a child's part shares (v282): where a word of rom.4's notes W is in the port's, the call struct at `at` as the
// port is given it, and a child's routine on both (the pool's words and W written into rom.4's memory, the logs
// cleared and the answers seeded alike, then rom.4's routine and the port's `mine()`; what differs in the pool, W, the
// calls or the answer)
uint32_t* setsPortWords(SetSides& s, uint32_t at);
Vc2SetCall setsCallAt(SetSides& s, uint32_t at);
std::string setsChildOnBoth(SetSides& s, const Vc2Region& pool, const Vc2CheckRoutine& routine,
                            std::initializer_list<uint32_t> args, const std::function<uint32_t()>& mine);

// A child's part's sides (v282): the sets' sides, the port's pool of the child, its counts (`events`, `calls` and
// `byEvent` at least)
template <class Pool, class Counts>
struct SetChildSides {
    explicit SetChildSides(SetSides& sides) : s(sides) {}
    SetSides& s;
    Pool port {};
    Counts n {};
    // One of the pool's objects
    uint32_t any(std::mt19937& rng) const { return Pool::kAt + Pool::kBytes * (uint32_t)(rng() % Pool::kCount); }
    // The pool as rom.4 holds it, said as `what`
    Vc2Region region(const char* what) const { return { Pool::kAt, &port.items, sizeof port.items, what }; }
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
// counts' kPool: the end (a bare return in rom.4), the update (an object and C's call), the conversion (and a block
// `Block` draws) and the copy (an object and a block; the port's member given the reads when it takes them)
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

// A child's part's case: noise in the pool, then 400 of `Events()`, each after `Draw` (all at the first), its calls
// counted; what differs, or nothing
template <auto Draw, auto Events, class Sides>
std::string setsChildCase(Sides& x, std::mt19937& rng) {
    setsNoise(x.port.items, rng);
    const auto events = Events();
    std::string what;
    for (int k = 0; k < 400 && what.empty(); ++k, ++x.n.events) {
        Draw(x, rng, k == 0);
        what = events[rng() % events.size()](x, rng);
        x.n.calls += (long)x.s.mine.size();
    }
    return what;
}

// A child's part on the seed `offset` after the run's: the sets' part's hooks on its routines taken off, its sides,
// its cases, its counts said by `Say`; how many of its cases differ
template <class Sides, auto Draw, auto Events, auto Say>
long setsChildPart(SetSides& s, const Vc2Run& run, uint32_t offset, std::span<const uint32_t> routines) {
    for (const uint32_t at : routines) s.fw.cpu.unhook(at);
    const auto x = std::make_unique<Sides>(s);
    const long different = vc2CheckPart(run, offset, { "XT_VC2SETS_FIRST", "XT_VC2SETS_STOP" }, x->n.events,
                                        [&](std::mt19937& rng) { return setsChildCase<Draw, Events>(*x, rng); });
    Say(x->n);
    return different;
}

// The further parts, each on the seed after the last's, saying its counts; how many of its cases differ: the common's
// (v281, Vc2SetCommonCheck.cpp)
long vc2SetCommonPart(SetSides& s, const Vc2Run& run);
// The envelopes' (v282, Vc2SetEnvelopeCheck.cpp)
long vc2SetEnvelopePart(SetSides& s, const Vc2Run& run);
// The further parts in turn
long vc2SetsFurther(SetSides& s, const Vc2Run& run);
