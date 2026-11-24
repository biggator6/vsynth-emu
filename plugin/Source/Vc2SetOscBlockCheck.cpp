// xt_vc2sets' eighth part (v391): C's OSC blocks, Vc2SetOscBlocks with Vc2SetOscKinds, against rom.4's own routines on
// its own blocks (0x89e34060), the step counts (0x89989c84), Random Pitch's seed (0x89996f4c) and notes W, after the COSM
// inputs' part on the seed after its.  The OSC's four Matrix getters are hooked to answer drawn groups, a place each,
// and the pitch source (0x89394a40) a drawn ratio, neither logged: rom.4 calls them more often than the port, which
// reads them once a converter.  PCM's start, which the port hasn't, is hooked to log its routine and do nothing
// (EXTIN's converter too until v393, PCM's until v394, its LITE and VariPhrase words until v395 and v396).  The velocity curve and powf are rom.4's own (the port's
// through SetKeptShared).  The voices (each one's glide key at its +0x38, the glide keys' tables' Hold), the parts, the
// parts' OSC table (0x89e371e0: the headers, records and systems), the clock, the make's legato structs and four Ms'
// bends and D-Beam bytes are drawn in rom.4's memory, which the port reads too (Vc2SetOscBlockDraws.cpp).  Each case
// noise in the blocks, then 400 events: the placement, the constructor, the end, the conversion, the update, the copy,
// the refresh, the dispatch and the converter alone; after each the blocks, the seed, the step counts, W, the calls and
// the answer.  Not drawn: a kind past 3 with an OSC past 1, or past 15 (the step counts end at 16 parts' 64 bytes), a
// Vibrato Type past 15 (its four bits).  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetOscBlockCheck.h"
#include "Vc2SetOscKinds.h"
#include "Vc2ToneInput.h"
#include "Vc2VocoderOsc.h"
#include <bit>

namespace {
using Blocks = Vc2SetOscBlocks;
constexpr uint32_t kGetters[] = { 0x893a23b0, 0x893a2440, 0x893a26b0, 0x893a28b0 }, kPitchSource = 0x89394a40;
constexpr uint32_t kRoutines[] = { 0x8939fdd0, 0x8939fe00, 0x893a0000, 0x893a0040, 0x893a0250, 0x893a0310, 0x893a0150 };
} // namespace

uint16_t SetPortParts::nextOscStep(uint32_t part, uint32_t kind, uint32_t osc) {
    uint16_t& step = oscSteps.at(32 * part + 2 * kind + osc);
    return step++;
}

Vc2MatrixGroups SetPortParts::oscGroups(uint32_t, uint32_t osc) {
    Vc2MatrixGroups g;
    const uint32_t second = osc == 0 ? 0 : 1;
    fw_.cpu.read(kOscGroupsAt + kOscGroupBytes * second, g.osc, sizeof g.osc);
    fw_.cpu.read(kOscGroupsAt + kOscGroupBytes * (2 + second), g.lfo, sizeof g.lfo);
    return g;
}

float SetPortParts::pitchSource() { return std::bit_cast<float>(fw_.cpu.rd32(kOscSourceAt)); }

const Vc2WaveMemory& SetPortParts::waveMemory() {
    memory_.reads = 0;
    return memory_;
}

uint8_t SetPortParts::FwMemory::byte(uint32_t at) const {
    constexpr long kBound = 65536;
    if (++reads > kBound) throw std::runtime_error("the wave memory: past 65,536 reads in one use");
    return fw_.cpu.rd8(at);
}

namespace {
// The events' counts, the store's three alone and the sets' each with how many found a record (loaded, a set's index,
// the words, the words by a set)
struct OscCounts : SetChildCounts<17> {
    static constexpr const char* kPool = "the OSC blocks";
    static constexpr const char* kSaid = "OSC blocks: %ld events: %ld placements, %ld constructors, %ld ends, %ld "
                                         "conversions, %ld updates, %ld copies, %ld refreshes, %ld dispatches, %ld "
                                         "converters, the store's %ld loaded (%ld yes), %ld indices (%ld a set's), %ld "
                                         "words (%ld found), the sets' %ld words (%ld found) (%ld calls out)\n";
};

// The sides, the pool's regions its seed, the step counts, the wave store's words and the sets besides (the port's in
// its parts)
struct OscSides : SetChildSides<Blocks, OscCounts> {
    using SetChildSides::SetChildSides;
    std::array<Vc2Region, 7> region(const char* what) const {
        const Vc2WaveSets& sets = s.parts.sets;
        return { Vc2Region { Blocks::kAt, &port.items, sizeof port.items, what },
                 Vc2Region { Blocks::kSeedAt, &port.seed, sizeof port.seed, "the seed" },
                 Vc2Region { kOscStepsAt, &s.parts.oscSteps, sizeof s.parts.oscSteps, "the step counts" },
                 Vc2Region { Vc2WaveStore::kAt, &s.parts.store.words, sizeof s.parts.store.words, "the store" },
                 Vc2Region { Vc2WaveSets::kCountsAt, &sets.counts, sizeof sets.counts, "the sets' counts" },
                 Vc2Region { Vc2WaveSets::kTablesAt, &sets.tables, sizeof sets.tables, "the sets' tables" },
                 Vc2Region { Vc2WaveSets::kBuffersAt, &sets.buffers, sizeof sets.buffers, "the sets' buffers" } };
    }
};
using OscEvent = std::string (*)(OscSides&, std::mt19937&);

uint32_t anyOscBlock(std::mt19937& rng) {
    const uint32_t note = setsNote(rng);
    return note + Vc2ToneInput::kOscs[rng() % 2];
}

// The constructor's kind: one of the four, now and then in a word with any other bits (rom.4 keeps its byte)
uint32_t oscKind(std::mt19937& rng) {
    const uint32_t kind = rng() % 4;
    return rng() % 4 ? kind : (((uint32_t)rng() & ~0xffu) | kind);
}

// The make's call: its word one of the legato structs
uint32_t oscCall(SetSides& s, std::mt19937& rng) {
    const uint32_t at = setsCall(s, rng);
    s.fw.cpu.wr32(at + 8, kOscLegatosAt + kOscLegatoBytes * (uint32_t)(rng() % kOscObjects));
    return at;
}

void oscDraw(OscSides& x, std::mt19937& rng, bool all) {
    setsDraw(x.s, rng, all);
    oscTableDrawn(x.s.fw, rng, all);
    oscAroundDrawn(x.s.fw, rng, all);
    if (all || rng() % 8 == 0) setsClockDrawn(x.s.fw, rng);
    for (Vc2SetOscInput& w : x.port.items)
        if (all || rng() % 8 == 0) oscInputDrawn(w, rng);
    oscSameRecordsDrawn(x.s.fw, x.port.items, rng);
    if (all || rng() % 8 == 0)
        for (uint16_t& step : x.s.parts.oscSteps) step = (uint16_t)(rng() % 4 ? rng() % 8 : rng());
    if (all || rng() % 8 == 0) x.port.seed.value = (uint16_t)rng();
    oscStoreDrawn(x.s.fw, x.s.parts.store, rng, all);   // v398
    oscSetsDrawn(x.s.fw, x.s.parts.sets, rng, all);     // v399
}

// v398: the store's routines alone, on a key or a Wave Number about the store's (now and then any, or 0), the words'
// into an OSC block's wave words (+0xa0); v399: the sets' on a set's number, a key and a velocity (0…127 mostly) too;
// each answer, and how many found a record, counted
constexpr uint32_t kWaveAt = 0xa0;   // [FW] the input's wave words
using StoreArgs = std::array<uint32_t, 3>;   // a key, number or set's number, then the key and velocity (the sets')

StoreArgs storeKey(std::mt19937& rng) {
    constexpr uint32_t kBase = 3000 - 1;   // [FW] a Wave Number's key: less 1, plus 3000
    return { (uint16_t)(rng() % 8 ? oscStoreNumber(rng) + kBase : rng()), 0, 0 };
}

StoreArgs storeNumber(std::mt19937& rng) {
    const auto numbers = std::to_array<uint32_t>({ oscStoreNumber(rng), 0, (uint16_t)rng() });
    return { numbers[rng() % 8 ? 0 : 1 + rng() % 2], 0, 0 };
}

uint32_t playedByte(std::mt19937& rng) { return (uint32_t)(rng() % 4 ? rng() % 128 : rng() % 256); }

StoreArgs setArgs(std::mt19937& rng) {
    const uint32_t number = oscSetNumber(rng), key = playedByte(rng);
    return { number, key, playedByte(rng) };
}

uint32_t loadedMine(OscSides& x, uint32_t, const StoreArgs& a) {
    return x.s.parts.store.loadedRecord(a[0], x.s.parts.waveMemory()) != 0 ? 1 : 0;
}

uint32_t indexMine(OscSides& x, uint32_t, const StoreArgs& a) {
    return (uint32_t)x.s.parts.store.setIndex(a[0], x.s.parts.waveMemory());
}

uint8_t* waveOf(OscSides& x, uint32_t object) { return reinterpret_cast<uint8_t*>(&x.port.at(object)) + kWaveAt; }

uint32_t wordsMine(OscSides& x, uint32_t object, const StoreArgs& a) {
    return vc2WaveWords(x.s.parts.store, (uint16_t)a[0], waveOf(x, object), x.s.parts.waveMemory()) ? 1 : 0;
}

uint32_t bySetMine(OscSides& x, uint32_t object, const StoreArgs& a) {
    SetPortParts& p = x.s.parts;
    return p.sets.waveWords((uint16_t)a[0], waveOf(x, object), (uint8_t)a[1], (uint8_t)a[2], p.store, p.waveMemory());
}

bool answered(uint32_t answer) { return answer != 0; }
bool indexed(uint32_t answer) { return (int32_t)answer != -1 && (int32_t)answer != -2; }   // [FW] none, no set

template <uint32_t Routine, size_t Counted, auto Drawn, auto Mine, auto Found>
std::string storeAlone(OscSides& x, std::mt19937& rng) {
    ++x.n.byEvent[Counted];
    const uint32_t object = x.any(rng);
    const StoreArgs a = Drawn(rng);
    uint32_t answer = 0;
    const std::string what = setsChildOnBoth(x.s, x.region(OscCounts::kPool), { Routine, "the store's", vc2R0 },
                                             { a[0], object + kWaveAt, a[1], a[2] },
                                             [&] { return answer = Mine(x, object, a); });
    if (Found(answer)) ++x.n.byEvent[Counted + 1];
    return what;
}

// The dispatch or the converter alone, on an input and a block
template <auto Fn, uint32_t Routine, size_t Counted>
std::string oscAlone(OscSides& x, std::mt19937& rng) {
    ++x.n.byEvent[Counted];
    const auto mine = [&](uint32_t object, uint32_t* block) { Fn(x.port.at(object), block, x.s.parts); };
    return setsChildCopy<OscSides, anyOscBlock>(x, rng, { Routine, "alone" }, OscCounts::kPool, mine);
}

// [FW] the events by routine
const OscEvent kEvents[] = {
    setsPlaceEvent<OscSides, 0x8939fdd0, 0x108, 0>,
    setsMakeEvent<OscSides, anyOscBlock, 0x8939fe00, 1, Blocks::kPerVoice, oscMatrixDrawn, oscKind, oscCall>,
    setsMakeEvent<OscSides, anyOscBlock, 0x8939fe00, 1, Blocks::kPerVoice, oscMatrixDrawn, oscKind, oscCall>,
    setsEndEvent<OscSides, 0x893a0000, 2>,
    setsConvertEvent<OscSides, &Blocks::convert, anyOscBlock, 0x893a0040, 3>,
    setsConvertEvent<OscSides, &Blocks::convert, anyOscBlock, 0x893a0040, 3>,
    setsUpdateEvent<OscSides, &Blocks::update, 0x893a0250, 4>,
    setsCopyEvent<OscSides, &Blocks::copy, anyOscBlock, 0x893a0310, 5>,
    setsRefreshEvent<OscSides, &Blocks::refresh, 0x893a0150, 6>,
    oscAlone<vc2SetOscStart, 0x8939fb40, 7>,
    oscAlone<vc2SetOscConvert, 0x8939fbd0, 8>,
    storeAlone<0x89351840, 9, storeKey, loadedMine, answered>,
    storeAlone<0x893520a0, 11, storeKey, indexMine, indexed>,
    storeAlone<0x8939cd30, 13, storeNumber, wordsMine, answered>,
    storeAlone<kOscBySetAt, 15, setArgs, bySetMine, answered>,
};

// rom.4's side: the getters a place each, the pitch source its ratio
void hookOsc(SetSides& s) {
    for (uint32_t k = 0; k < 4; ++k)
        s.fw.cpu.hook(kGetters[k], [k](Sh4& c) { c.ret(kOscGroupsAt + kOscGroupBytes * k); });
    s.fw.cpu.hook(kPitchSource, [](Sh4& c) {
        c.fb[0][0] = c.rd32(kOscSourceAt);
        c.pc = c.pr;
    });
}
} // namespace

long vc2SetOscBlockPart(SetSides& s, const Vc2Run& run) {
    hookOsc(s);
    return setsChildPart<OscSides, oscDraw, kEvents>(s, run, 7, kRoutines);
}
#endif
