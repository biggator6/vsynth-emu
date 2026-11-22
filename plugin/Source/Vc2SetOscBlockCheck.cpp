// xt_vc2sets' eighth part (v391): C's OSC blocks, Vc2SetOscBlocks with Vc2SetOscKinds, against rom.4's own routines on
// its own blocks (0x89e34060), the step counts (0x89989c84), Random Pitch's seed (0x89996f4c) and notes W, after the COSM
// inputs' part on the seed after its.  The OSC's four Matrix getters are hooked to answer drawn groups, a place each,
// and the pitch source (0x89394a40) a drawn ratio, neither logged: rom.4 calls them more often than the port, which
// reads them once a converter.  PCM's start and the PCM converter, which the port hasn't, are hooked to log their
// routine and do nothing (EXTIN's converter too until v393).  The velocity curve and powf are rom.4's own (the port's
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
constexpr uint32_t kUnported[] = { 0x8939e120, 0x8939e300 };   // v393: EXTIN's converter 0x8939e9e0 ported
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

void SetPortParts::unportedOscKind(uint32_t routine, uint8_t*, uint32_t*) { logged(kSetOscKind, routine, {}); }

namespace {
struct OscCounts : SetChildCounts<9> {
    static constexpr const char* kPool = "the OSC blocks";
    static constexpr const char* kSaid = "OSC blocks: %ld events: %ld placements, %ld constructors, %ld ends, %ld "
                                         "conversions, %ld updates, %ld copies, %ld refreshes, %ld dispatches, %ld "
                                         "converters (%ld calls of the kinds' routines not ported)\n";
};

// The sides, the pool's regions its seed and the step counts besides (the port's in its parts)
struct OscSides : SetChildSides<Blocks, OscCounts> {
    using SetChildSides::SetChildSides;
    std::array<Vc2Region, 3> region(const char* what) const {
        return { Vc2Region { Blocks::kAt, &port.items, sizeof port.items, what },
                 Vc2Region { Blocks::kSeedAt, &port.seed, sizeof port.seed, "the seed" },
                 Vc2Region { kOscStepsAt, &s.parts.oscSteps, sizeof s.parts.oscSteps, "the step counts" } };
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
};

// rom.4's side: the getters a place each, the pitch source its ratio, the kinds' routines not ported logged
void hookOsc(SetSides& s) {
    for (uint32_t k = 0; k < 4; ++k)
        s.fw.cpu.hook(kGetters[k], [k](Sh4& c) { c.ret(kOscGroupsAt + kOscGroupBytes * k); });
    s.fw.cpu.hook(kPitchSource, [](Sh4& c) {
        c.fb[0][0] = c.rd32(kOscSourceAt);
        c.pc = c.pr;
    });
    for (const uint32_t at : kUnported)
        s.fw.cpu.hook(at, [&s, at](Sh4& c) {
            s.rom.push_back({ kSetOscKind, at, {} });
            c.ret(0);
        });
}
} // namespace

long vc2SetOscBlockPart(SetSides& s, const Vc2Run& run) {
    hookOsc(s);
    return setsChildPart<OscSides, oscDraw, kEvents>(s, run, 7, kRoutines);
}
#endif
