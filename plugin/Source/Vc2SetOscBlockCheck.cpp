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

std::array<uint8_t, 32> SetPortParts::oscHeader(uint32_t part) { return bytesAt<32>(kOscTableAt + kOscPartRow * part); }

std::array<uint8_t, 128> SetPortParts::oscRecord(uint32_t part, uint32_t kind, uint32_t osc) {
    const uint32_t slot = kOscHeaderBytes + kOscKindRow * kind + kOscRecordBytes * osc;
    return bytesAt<128>(kOscTableAt + kOscPartRow * part + slot);
}

std::array<uint8_t, 60> SetPortParts::oscSystems() { return bytesAt<60>(kOscSystemsAt); }

uint16_t& SetPortParts::oscStep(uint32_t part, uint32_t kind, uint32_t osc) {
    return oscSteps.at(32 * part + 2 * kind + osc);
}

Vc2SetGlide SetPortParts::glideAt(uint32_t glide) {
    const auto b = bytesAt<0x24>(glide);
    Vc2SetGlide g;
    std::memcpy(&g.table, b.data(), sizeof g.table);
    g.prevPoly = b[0x1a];
    g.prevPolyHeld = b[0x1b];
    g.prevMono = b[0x20];
    g.prevMonoHeld = b[0x21];
    return g;
}

uint8_t SetPortParts::holdOf(uint32_t keys) { return fw_.cpu.rd8(keys + kOscHoldAt); }

std::array<uint8_t, 2> SetPortParts::legatoOf(uint32_t word) {
    return { fw_.cpu.rd8(word + 1), fw_.cpu.rd8(word + 2) };
}

int32_t SetPortParts::matrixBend(uint32_t matrix) { return static_cast<int32_t>(fw_.cpu.rd32(matrix + kOscBendAt)); }

Vc2MatrixGroups SetPortParts::oscGroups(uint32_t, uint32_t osc) {
    Vc2MatrixGroups g;
    const uint32_t second = osc == 0 ? 0 : 1;
    fw_.cpu.read(kOscGroupsAt + kOscGroupBytes * second, g.osc, sizeof g.osc);
    fw_.cpu.read(kOscGroupsAt + kOscGroupBytes * (2 + second), g.lfo, sizeof g.lfo);
    return g;
}

float SetPortParts::pitchSource() { return std::bit_cast<float>(fw_.cpu.rd32(kOscSourceAt)); }

uint32_t SetPortParts::waveBySet(std::array<uint32_t, 3> args, uint8_t* wave) {
    logged(kSetOscKind, kOscBySetAt, { args[0], args[1], args[2] });
    return oscBySetAnswered(wave, answers_);
}

uint32_t SetPortParts::tripMoving(uint8_t part) {
    return fw_.cpu.rd32(fw_.cpu.rd32(kOscPartTableAt + 4u * part) + kOscTripMovingAt);
}

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
// The events' counts, the store's three alone each with how many found a record (loaded, a set's index, the words)
struct OscCounts : SetChildCounts<15> {
    static constexpr const char* kPool = "the OSC blocks";
    static constexpr const char* kSaid = "OSC blocks: %ld events: %ld placements, %ld constructors, %ld ends, %ld "
                                         "conversions, %ld updates, %ld copies, %ld refreshes, %ld dispatches, %ld "
                                         "converters, the store's %ld loaded (%ld yes), %ld indices (%ld a set's), %ld "
                                         "words (%ld found) (%ld calls of 0x8939cba0)\n";
};

// The sides, the pool's regions its seed, the step counts and the wave store's words besides (the port's in its parts)
struct OscSides : SetChildSides<Blocks, OscCounts> {
    using SetChildSides::SetChildSides;
    std::array<Vc2Region, 4> region(const char* what) const {
        return { Vc2Region { Blocks::kAt, &port.items, sizeof port.items, what },
                 Vc2Region { Blocks::kSeedAt, &port.seed, sizeof port.seed, "the seed" },
                 Vc2Region { kOscStepsAt, &s.parts.oscSteps, sizeof s.parts.oscSteps, "the step counts" },
                 Vc2Region { Vc2WaveStore::kAt, &s.parts.store.words, sizeof s.parts.store.words, "the store" } };
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
}

// v398: the store's routines alone, on a key or a Wave Number about the store's (now and then any, or 0), the words'
// into an OSC block's wave words (+0xa0); each answer, and how many found a record, counted
constexpr uint32_t kWaveAt = 0xa0;   // [FW] the input's wave words

uint32_t storeKey(std::mt19937& rng) {
    constexpr uint32_t kBase = 3000 - 1;   // [FW] a Wave Number's key: less 1, plus 3000
    return (uint16_t)(rng() % 8 ? oscStoreNumber(rng) + kBase : rng());
}

uint32_t storeNumber(std::mt19937& rng) {
    const auto numbers = std::to_array<uint32_t>({ oscStoreNumber(rng), 0, (uint16_t)rng() });
    return numbers[rng() % 8 ? 0 : 1 + rng() % 2];
}

uint32_t loadedMine(OscSides& x, uint32_t, uint32_t key) {
    return x.s.parts.store.loadedRecord(key, x.s.parts.waveMemory()) != 0 ? 1 : 0;
}

uint32_t indexMine(OscSides& x, uint32_t, uint32_t key) {
    return (uint32_t)x.s.parts.store.setIndex(key, x.s.parts.waveMemory());
}

uint32_t wordsMine(OscSides& x, uint32_t object, uint32_t number) {
    uint8_t* wave = reinterpret_cast<uint8_t*>(&x.port.at(object)) + kWaveAt;
    return vc2WaveWords(x.s.parts.store, (uint16_t)number, wave, x.s.parts.waveMemory()) ? 1 : 0;
}

bool answered(uint32_t answer) { return answer != 0; }
bool indexed(uint32_t answer) { return (int32_t)answer != -1 && (int32_t)answer != -2; }   // [FW] none, no set

template <uint32_t Routine, size_t Counted, auto Drawn, auto Mine, auto Found>
std::string storeAlone(OscSides& x, std::mt19937& rng) {
    ++x.n.byEvent[Counted];
    const uint32_t object = x.any(rng), arg = Drawn(rng);
    uint32_t answer = 0;
    const std::string what = setsChildOnBoth(x.s, x.region(OscCounts::kPool), { Routine, "the store's", vc2R0 },
                                             { arg, object + kWaveAt }, [&] { return answer = Mine(x, object, arg); });
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
};

// The words by a set's index on rom.4's side (v397): logged with the arguments the port gives (the index, the key and
// velocity), answered, and the wave's words at r5 filled, from the same answers as the port's
void bySetHooked(SetSides& s, Sh4& c) {
    s.rom.push_back({ kSetOscKind, kOscBySetAt, { c.r[4], c.r[6], c.r[7] } });
    std::array<uint8_t, kOscWaveBytes> wave {};
    c.read(c.r[5], wave.data(), wave.size());
    const uint32_t answer = oscBySetAnswered(wave.data(), s.romAnswers);
    c.write(c.r[5], wave.data(), wave.size());
    c.ret(answer);
}

// rom.4's side: the getters a place each, the pitch source its ratio, the words by a set's index as the port's parts
void hookOsc(SetSides& s) {
    for (uint32_t k = 0; k < 4; ++k)
        s.fw.cpu.hook(kGetters[k], [k](Sh4& c) { c.ret(kOscGroupsAt + kOscGroupBytes * k); });
    s.fw.cpu.hook(kPitchSource, [](Sh4& c) {
        c.fb[0][0] = c.rd32(kOscSourceAt);
        c.pc = c.pr;
    });
    s.fw.cpu.hook(kOscBySetAt, [&s](Sh4& c) { bySetHooked(s, c); });
}
} // namespace

long vc2SetOscBlockPart(SetSides& s, const Vc2Run& run) {
    hookOsc(s);
    return setsChildPart<OscSides, oscDraw, kEvents>(s, run, 7, kRoutines);
}
#endif
