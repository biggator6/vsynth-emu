// xt_vc2voices' events (v272): each of the voices' routines on both sides, rom.4's then the port's, and what differs
// after: the span of the voices' words, the calls and the answer.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2PartVoicesCheck.h"
#include "Vc2PartVoicesTables.h"
#include <bit>

namespace {
using V = Vc2PartVoices;
using Words = std::array<uint32_t, 2>;

// How rom.4's answer reads: its r0, its fr0, or the two words a getter wrote; none when the routine answers nothing
using Theirs = Words (*)(Vc2Fw&);
Words romR0(Vc2Fw& fw) { return { fw.cpu.r[0] }; }
Words romFr0(Vc2Fw& fw) { return { fw.cpu.fb[0][0] }; }
Words romOut(Vc2Fw& fw) { return { fw.cpu.rd32(kVoicesOutAt), fw.cpu.rd32(kVoicesOutAt + 4) }; }
constexpr Theirs kNone = nullptr;

Words countWords(Vc2VoiceCount c) { return { (uint32_t)c.count, std::bit_cast<uint32_t>(c.sum) }; }
Words floatWords(float x) { return { std::bit_cast<uint32_t>(x) }; }

// rom.4's `routine` on `args`, then the port's `mine()` giving its answer's words; then what differs, the answers last
template <class Mine>
std::string onVoices(VoicesSides& s, uint32_t routine, std::initializer_list<uint32_t> args, const char* name,
                     Theirs theirs, Mine mine) {
    Words ours {};
    return vc2BothOn(s.fw, routine, args, name, [&] { ours = mine(); }, [&] {
        const std::string what = voicesDifference(s);
        if (!what.empty() || theirs == kNone) return what;
        const Words rom = theirs(s.fw);
        if (rom == ours) return what;
        char t[128];
        std::snprintf(t, sizeof t, "%s answers %08x %08x, against %08x %08x", name, ours[0], ours[1], rom[0], rom[1]);
        return std::string(t);
    });
}

// A routine answering nothing on the port's side
template <class Mine>
std::string onVoid(VoicesSides& s, uint32_t routine, std::initializer_list<uint32_t> args, const char* name,
                   Mine mine) {
    return onVoices(s, routine, args, name, kNone, [&] {
        mine();
        return Words {};
    });
}

uint32_t anyVoice(VoicesSides& s, std::mt19937& rng) { return s.port.addressOf((int)(rng() % V::kVoices)); }

// A sounding voice, or none one time in five
uint32_t soundingOrNone(VoicesSides& s, std::mt19937& rng) {
    const std::vector<uint32_t> voices = voicesOf(s, s.port.g.lists[V::kSounding]);
    return voices.empty() || rng() % 5 == 0 ? 0 : voices[rng() % voices.size()];
}

// A list whose voices all have notes (the sounding list always has)
uint32_t notedList(VoicesSides& s, std::mt19937& rng) {
    const auto noted = [&](uint32_t v) { return s.port.at(v).note != 0; };
    std::vector<uint32_t> lists;
    for (const uint32_t list : s.port.g.lists)
        if (std::ranges::all_of(voicesOf(s, list), noted)) lists.push_back(list);
    return lists[rng() % lists.size()];
}

std::string onVoiceTake(VoicesSides& s, std::mt19937&) {
    const uint32_t free = s.port.g.lists[V::kFree];
    const bool fromFree = s.port.at(free).next != free;
    uint32_t got = 0;
    std::string what = onVoices(s, 0x893905a0, {}, "the take", romR0, [&] {   // [FW]
        got = s.port.take(s.parts);
        return Words { got };
    });
    ++s.n.takes;
    s.n.fromFree += fromFree;
    s.n.stolen += !fromFree && got;
    s.n.none += got == 0;
    return what;
}

std::string onOldest(VoicesSides& s, std::mt19937&) {
    ++s.n.stops;
    return onVoices(s, 0x893906c0, {}, "the oldest", romR0, [&] { return Words { s.port.oldest(s.parts) }; });   // [FW]
}

std::string onStop(VoicesSides& s, std::mt19937& rng) {
    const uint32_t v = soundingOrNone(s, rng);
    ++s.n.stops;
    return onVoices(s, 0x89390700, { v }, "the stop", romR0, [&] { return Words { s.port.stop(v, s.parts) }; });   // [FW]
}

// A move to prepared with its state: 0 as the take gives it, 1, or any word (rom.4 keeps its low byte)
std::string onToPrepared(VoicesSides& s, std::mt19937& rng) {
    const uint32_t v = anyVoice(s, rng), state = rng() % 4 == 0 ? (uint32_t)rng() : rng() % 3 == 0 ? 1 : 0;
    ++s.n.moves;
    return onVoid(s, 0x893919b0, { v, state }, "the move to prepared",   // [FW]
                  [&] { s.port.toPrepared(v, (uint8_t)state, s.parts); });
}

// A routine of the port's on one argument, and rom.4's, by name
template <class Arg>
struct Routine {
    uint32_t at;
    const char* name;
    void (V::*mine)(Arg, Vc2VoiceParts&);
};
constexpr Routine<uint32_t> kToFree { 0x893918e0, "the move to free", &V::toFree };   // [FW]
constexpr Routine<uint32_t> kToReserved { 0x89391930, "the move to reserved", &V::toReserved };   // [FW]

// A move of any voice to free or reserved
template <const Routine<uint32_t>& R>
std::string onStopped(VoicesSides& s, std::mt19937& rng) {
    const uint32_t v = anyVoice(s, rng);
    ++s.n.moves;
    return onVoid(s, R.at, { v }, R.name, [&] { (s.port.*R.mine)(v, s.parts); });
}

// The reserve: half the time as its caller clamps it (0x89391550: the offset to 23, the count to what is left), else
// either past
std::string onReserve(VoicesSides& s, std::mt19937& rng) {
    const bool clamped = rng() % 2;
    const auto offset = (int32_t)(rng() % (clamped ? 24 : 31));
    const auto count = (int32_t)(rng() % (uint32_t)(clamped ? 25 - offset : 31));
    ++s.n.reserves;
    return onVoid(s, 0x893911b0, { (uint32_t)count, (uint32_t)offset }, "the reserve",   // [FW]
                  [&] { s.port.reserve(count, offset, s.parts); });
}

// A routine of the port's on the voices alone, and rom.4's, by name, and the count it adds to
struct ListPass {
    uint32_t at;
    const char* name;
    void (V::*mine)(Vc2VoiceParts&);
    long VoicesCounts::*count;
};
constexpr ListPass kEndedScan { 0x89390ee0, "the ended scan", &V::endedScan, &VoicesCounts::scans };   // [FW]
constexpr ListPass kStopAll { 0x89390f40, "stop all", &V::stopAll, &VoicesCounts::stopAlls };   // [FW]

template <const ListPass& P>
std::string onListPass(VoicesSides& s, std::mt19937&) {
    ++(s.n.*P.count);
    return onVoid(s, P.at, {}, P.name, [&] { (s.port.*P.mine)(s.parts); });
}

// A voice's cost, of one with a note; the sounding list's when none has
std::string onVoiceCost(VoicesSides& s, std::mt19937& rng) {
    std::vector<uint32_t> noted;
    for (int i = 0; i < V::kVoices; ++i)
        if (s.port.at(s.port.addressOf(i)).note != 0) noted.push_back(s.port.addressOf(i));
    const uint32_t v = noted.empty() ? s.port.g.lists[V::kSounding] : noted[rng() % noted.size()];
    const uint32_t routine = noted.empty() ? 0x89390630 : 0x89390610;   // [FW] a list's cost, a voice's
    ++s.n.costs;
    return onVoices(s, routine, { v }, "a voice's cost", romFr0, [&] {
        return floatWords(noted.empty() ? s.port.costOf(v, s.parts) : s.port.cost(v, s.parts));
    });
}

std::string onListCost(VoicesSides& s, std::mt19937& rng) {
    const uint32_t list = notedList(s, rng);
    ++s.n.costs;
    return onVoices(s, 0x89390630, { list }, "a list's cost", romFr0,   // [FW]
                    [&] { return floatWords(s.port.costOf(list, s.parts)); });
}

std::string onSoundingCost(VoicesSides& s, std::mt19937&) {
    ++s.n.costs;
    return onVoices(s, 0x893912e0, { kVoicesOutAt }, "the sounding cost", romOut,   // [FW]
                    [&] { return countWords(s.port.sounding(s.parts)); });
}

// CPU Power Max for TG or Offset for Voice: its usual values (600, less 400 by Category), its edges, or any
int32_t drawPower(std::mt19937& rng) {
    constexpr int32_t kPowers[6] = { 600, 200, 0, -400, 1, 2000 };
    return rng() % 3 == 0 ? (int32_t)rng() : rng() % 2 ? kPowers[rng() % 6] : (int32_t)(rng() % 3000);
}

// A setter of the budget's, and rom.4's, by name
struct Setter {
    uint32_t at;
    const char* name;
    void (V::*mine)(int32_t);
};
constexpr Setter kSetBudget { 0x89391240, "the budget's setter", &V::setBudget };   // [FW]
constexpr Setter kSetOffset { 0x89391260, "the offset's setter", &V::setOffset };   // [FW]

template <const Setter& S>
std::string onSet(VoicesSides& s, std::mt19937& rng) {
    const int32_t power = drawPower(rng);
    ++s.n.getters;
    return onVoid(s, S.at, { (uint32_t)power }, S.name, [&] { (s.port.*S.mine)(power); });
}

std::string onBudget(VoicesSides& s, std::mt19937&) {
    ++s.n.getters;
    return onVoices(s, 0x893912d0, { kVoicesOutAt }, "the budget", romOut,   // [FW]
                    [&] { return countWords(s.port.budget()); });
}

std::string onPreparedAny(VoicesSides& s, std::mt19937&) {
    ++s.n.getters;
    return onVoices(s, 0x89390850, {}, "the prepared test", romR0,   // [FW]
                    [&] { return Words { s.port.preparedAny() }; });
}

// An init of the port's, and rom.4's, by name
struct Init {
    uint32_t at;
    const char* name;
    void (V::*mine)();
};
constexpr Init kStaticInit { 0x89391450, "the static initializer", &V::staticInit };   // [FW]
constexpr Init kInitVoices { 0x89391ba0, "the voices' init", &V::initVoices };   // [FW]
constexpr Init kInitLists { 0x89391270, "the lists' init", &V::initLists };   // [FW]

constexpr const Init* kInits[3] = { &kStaticInit, &kInitVoices, &kInitLists };   // in the order rom.4 runs them

// The inits in turn, each after the last's words went to rom.4: what differs, or nothing
std::string onInits(VoicesSides& s, std::span<const Init* const> inits) {
    ++s.n.inits;
    std::string what;
    for (const Init* i : inits) {
        if (!what.empty()) break;
        voicesPrepare(s, 0);
        what = onVoid(s, i->at, {}, i->name, [&] { (s.port.*i->mine)(); });
    }
    return what;
}

// An init again: the static initializer, the lists', or the voices' and the lists' (the part's init's two: the voices'
// alone would leave sounding voices with no note), each a run of kInits: its first and its count
std::string onAgain(VoicesSides& s, std::mt19937& rng) {
    constexpr std::array<std::pair<size_t, size_t>, 3> kRuns { { { 0, 1 }, { 2, 1 }, { 1, 2 } } };
    const auto [first, count] = kRuns[rng() % kRuns.size()];
    return onInits(s, std::span(kInits).subspan(first, count));
}

// Each kind's share of the events: the take and the moves the most, as the notes' task runs them
const VoicesEvent kEvents[] = {
    onVoiceTake, onVoiceTake, onVoiceTake, onVoiceTake, onOldest, onStop, onStop, onToPrepared, onStopped<kToFree>,
    onStopped<kToFree>, onStopped<kToReserved>, onReserve, onReserve, onListPass<kEndedScan>,
    onListPass<kEndedScan>, onListPass<kStopAll>, onVoiceCost, onListCost, onSoundingCost, onSet<kSetBudget>,
    onSet<kSetOffset>, onBudget, onPreparedAny, onAgain,
};
} // namespace

std::span<const VoicesEvent> voicesEvents() { return kEvents; }

std::string voicesInits(VoicesSides& s) { return onInits(s, kInits); }
#endif
