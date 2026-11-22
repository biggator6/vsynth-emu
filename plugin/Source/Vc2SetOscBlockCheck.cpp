// xt_vc2sets' eighth part (v391): C's OSC blocks, Vc2SetOscBlocks with Vc2SetOscKinds, against rom.4's own routines on
// its own blocks (0x89e34060), the step counts (0x89989c84), Random Pitch's seed (0x89996f4c) and notes W, after the COSM
// inputs' part on the seed after its.  The OSC's four Matrix getters are hooked to answer drawn groups, a place each,
// and the pitch source (0x89394a40) a drawn ratio, neither logged: rom.4 calls them more often than the port, which
// reads them once a converter.  PCM's start and the PCM and EXTIN converters, which the port hasn't, are hooked to log
// their routine and do nothing.  The velocity curve and powf are rom.4's own (the port's through SetKeptShared).  The
// voices (each one's glide key at its +0x38, the glide keys' tables' Hold), the parts, the parts' OSC table (0x89e371e0:
// the headers, records and systems), the clock, the make's legato structs and four Ms' bends and D-Beam bytes are drawn
// in rom.4's memory, which the port reads too.  Each case noise in the blocks, then 400 events: the placement, the
// constructor, the end, the conversion, the update, the copy, the refresh, the dispatch and the converter alone; after
// each the blocks, the seed, the step counts, W, the calls and the answer.  Not drawn: a kind past 3 with an OSC past 1,
// or past 15 (the step counts end at 16 parts' 64 bytes), a Vibrato Type past 15 (its four bits).  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetOscBlock.h"
#include "Vc2SetOscKinds.h"
#include "Vc2SetsChildCheck.h"
#include "Vc2ToneInput.h"
#include "Vc2VocoderOsc.h"
#include <bit>

namespace {
using Blocks = Vc2SetOscBlocks;
// In Vc2Fw's RAM: the glide keys and their key tables, the legato structs, the four getters' groups, the pitch source
constexpr uint32_t kGlidesAt = 0x8C037000, kGlideBytes = 0x40, kGlideTablesAt = 0x8C038000, kGlideTableBytes = 0x200;
constexpr uint32_t kLegatosAt = 0x8C039000, kLegatoBytes = 4, kGroupsAt = 0x8C03A000, kGroupBytes = 0x20;
constexpr uint32_t kSourceAt = 0x8C03B000;
constexpr uint32_t kObjects = 4, kParts = 16, kRecordsAPart = 32;
// [FW] the parts' OSC table: a part's row, its header, a kind's and a record's bytes, the systems after 16 rows
constexpr uint32_t kTableAt = 0x89e371e0, kPartRow = 0x1020, kHeaderBytes = 32, kKindRow = 0x100, kRecordBytes = 0x80;
constexpr uint32_t kSystemsAt = kTableAt + kParts * kPartRow, kStepsAt = 0x89989c84;
constexpr uint32_t kGetters[] = { 0x893a23b0, 0x893a2440, 0x893a26b0, 0x893a28b0 }, kPitchSource = 0x89394a40;
constexpr uint32_t kUnported[] = { 0x8939e120, 0x8939e300, 0x8939e9e0 };
constexpr uint32_t kRoutines[] = { 0x8939fdd0, 0x8939fe00, 0x893a0000, 0x893a0040, 0x893a0250, 0x893a0310, 0x893a0150 };
constexpr uint32_t kHoldAt = 0x11c, kBendAt = 0x174, kBeamAt = 0x180, kSourceWordAt = 4;   // [FW]
constexpr uint32_t kAliasCancelAt = 9;   // [FW] the systems' Alias Cancel (+0x10209)
constexpr int kOscSums = 4, kLfoSums = 5;
} // namespace

std::array<uint8_t, 32> SetPortParts::oscHeader(uint32_t part) { return bytesAt<32>(kTableAt + kPartRow * part); }

std::array<uint8_t, 128> SetPortParts::oscRecord(uint32_t part, uint32_t kind, uint32_t osc) {
    return bytesAt<128>(kTableAt + kPartRow * part + kHeaderBytes + kKindRow * kind + kRecordBytes * osc);
}

std::array<uint8_t, 60> SetPortParts::oscSystems() { return bytesAt<60>(kSystemsAt); }

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

uint8_t SetPortParts::holdOf(uint32_t keys) { return fw_.cpu.rd8(keys + kHoldAt); }

std::array<uint8_t, 2> SetPortParts::legatoOf(uint32_t word) {
    return { fw_.cpu.rd8(word + 1), fw_.cpu.rd8(word + 2) };
}

int32_t SetPortParts::matrixBend(uint32_t matrix) { return static_cast<int32_t>(fw_.cpu.rd32(matrix + kBendAt)); }

Vc2MatrixGroups SetPortParts::oscGroups(uint32_t, uint32_t osc) {
    Vc2MatrixGroups g;
    const uint32_t second = osc == 0 ? 0 : 1;
    fw_.cpu.read(kGroupsAt + kGroupBytes * second, g.osc, sizeof g.osc);
    fw_.cpu.read(kGroupsAt + kGroupBytes * (2 + second), g.lfo, sizeof g.lfo);
    return g;
}

float SetPortParts::pitchSource() { return std::bit_cast<float>(fw_.cpu.rd32(kSourceAt)); }

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
                 Vc2Region { kStepsAt, &s.parts.oscSteps, sizeof s.parts.oscSteps, "the step counts" } };
    }
};
using OscEvent = std::string (*)(OscSides&, std::mt19937&);

uint32_t anyOscBlock(std::mt19937& rng) {
    const uint32_t note = setsNote(rng);
    return note + Vc2ToneInput::kOscs[rng() % 2];
}

uint32_t oscMatrix(std::mt19937& rng) {
    return kSetsTvaMatricesAt + kSetsTvaMatrixBytes * (uint32_t)(rng() % kObjects);
}

// The constructor's kind: one of the four, now and then in a word with any other bits (rom.4 keeps its byte)
uint32_t oscKind(std::mt19937& rng) {
    const uint32_t kind = rng() % 4;
    return rng() % 4 ? kind : (((uint32_t)rng() & ~0xffu) | kind);
}

// The make's call: its word one of the legato structs
uint32_t oscCall(SetSides& s, std::mt19937& rng) {
    const uint32_t at = setsCall(s, rng);
    s.fw.cpu.wr32(at + 8, kLegatosAt + kLegatoBytes * (uint32_t)(rng() % kObjects));
    return at;
}

void bytesDrawn(uint8_t* b, uint32_t n, std::mt19937& rng) {
    for (uint32_t i = 0; i < n; ++i) b[i] = (uint8_t)rng();
}

// A record: noise; the OSC on mostly, its type spread over the four and now and then any; the waveform one of the
// eleven, Pitch KF 0 or 100% now and then, a Vibrato Type of its four (now and then of its four bits), Random Pitch
// Depth none half the time
void recordDrawn(uint8_t* r, std::mt19937& rng) {
    bytesDrawn(r, kRecordBytes, rng);
    r[0] = (uint8_t)(rng() % 4 ? 1 : rng() % 2 ? 0 : rng());
    const std::array<uint8_t, 6> types { 0, 0, 3, 1, 2, (uint8_t)rng() };
    r[1] = setsOneOf(types, rng);
    if (rng() % 4) r[0x20] = (uint8_t)(rng() % 11);
    if (rng() % 4 == 0) {
        const uint16_t kf = rng() % 2 ? 0 : 1000;
        std::memcpy(r + 6, &kf, sizeof kf);
    }
    r[0x1c] = (uint8_t)(rng() % 8 ? rng() % 4 : rng() % 16);
    if (rng() % 2) std::memset(r + 0x12, 0, 2);
}

// A part's header: noise, mono and the portamento's byte mostly 0 or 1, the key's offset and coarse tune small
void headerDrawn(uint8_t* h, std::mt19937& rng) {
    bytesDrawn(h, kHeaderBytes, rng);
    h[4] = (uint8_t)(rng() % 4 ? rng() % 2 : rng());
    h[5] = (uint8_t)(rng() % 4 ? rng() % 3 : rng());
    h[0] = (uint8_t)(rng() % 4 ? (int)(rng() % 97) - 48 : (int)rng());
    h[1] = (uint8_t)(rng() % 4 ? (int)(rng() % 97) - 48 : (int)rng());
}

// The parts' OSC table: every header and record at a case's first draw, now and then a part's; the systems noise
void tableDrawn(Vc2Fw& fw, std::mt19937& rng, bool all) {
    for (uint32_t p = 0; p < kParts; ++p) {
        if (!all && rng() % 16) continue;
        std::array<uint8_t, kPartRow> row;
        headerDrawn(row.data(), rng);
        for (uint32_t k = 0; k < kRecordsAPart; ++k) recordDrawn(row.data() + kHeaderBytes + kRecordBytes * k, rng);
        fw.cpu.write(kTableAt + kPartRow * p, row.data(), sizeof row);
    }
    if (all || rng() % 8 == 0) {
        std::array<uint8_t, 60> sys;
        setsNoise(sys, rng);
        if (rng() % 2) sys[kAliasCancelAt] = 0;   // Alias Cancel off half the time, which noise nearly never is
        fw.cpu.write(kSystemsAt, sys.data(), sizeof sys);
    }
}

// The glide keys (every voice's +0x38 one of them), their tables' Hold, the legato structs, the Ms' bends and D-Beam
// bytes, the groups and the pitch source
void aroundDrawn(Vc2Fw& fw, std::mt19937& rng, bool all) {
    for (uint32_t j = 0; j < kObjects; ++j) {
        if (!(all || rng() % 4 == 0)) continue;
        std::array<uint8_t, 0x24> g;
        setsNoise(g, rng);
        const uint32_t table = kGlideTablesAt + kGlideTableBytes * j;
        std::memcpy(g.data(), &table, sizeof table);
        g[0x1b] = (uint8_t)(rng() % 4 ? rng() % 2 : rng());
        g[0x21] = (uint8_t)(rng() % 4 ? rng() % 2 : rng());
        fw.cpu.write(kGlidesAt + kGlideBytes * j, g.data(), sizeof g);
        fw.cpu.wr8(table + kHoldAt, setsByteDrawn<0, 63, 64, 127, 128>(rng));
        fw.cpu.wr32(kLegatosAt + kLegatoBytes * j, (uint32_t)rng());
        const uint32_t m = kSetsTvaMatricesAt + kSetsTvaMatrixBytes * j;
        fw.cpu.wr32(m + kBendAt, rng() % 2 ? 0 : rng() % 2 ? (uint32_t)(rng() % 4001) - 2000 : (uint32_t)rng());
        for (uint32_t b = 0; b < 4; ++b) fw.cpu.wr8(m + kBeamAt + b, (uint8_t)(rng() % 2 ? rng() % 128 : rng()));
    }
    for (uint32_t j = 0; j < kObjects; ++j)
        fw.cpu.wr32(kSetsVoicesAt + kSetsVoiceBytes * j + kSetsVoiceWords + kSourceWordAt,
                    kGlidesAt + kGlideBytes * (uint32_t)(rng() % kObjects));
    for (uint32_t k = 0; k < 4; ++k) {
        if (!(all || rng() % 4 == 0)) continue;
        for (int i = 0; i < (k < 2 ? kOscSums : kLfoSums); ++i) {
            const auto sums = std::to_array<int16_t>({ 0, (int16_t)(rng() % 141 - 70), (int16_t)(rng() % 26001 - 13000),
                                                       (int16_t)rng() });
            fw.cpu.wr16(kGroupsAt + kGroupBytes * k + 2 * i, (uint16_t)setsOneOf(sums, rng));
        }
    }
    if (all || rng() % 4 == 0) {
        const float source = rng() % 2 ? 1.0f : 0.25f + (float)(rng() % 3751) / 1000.0f;
        fw.cpu.wr32(kSourceAt, std::bit_cast<uint32_t>(source));
    }
}

// An input: noise; its part, kind and OSC where the step counts and records are drawn; a record; one of the Ms
void inputDrawn(Vc2SetOscInput& w, std::mt19937& rng) {
    setsNoise(w, rng);
    w.part = (uint8_t)(rng() % kParts);
    w.kind = (uint8_t)(rng() % 4 ? rng() % 4 : rng() % 16);
    w.osc = (uint8_t)(w.kind > 3 || rng() % 8 ? rng() % 2 : rng() % 16);
    recordDrawn(w.record.data(), rng);
    w.matrix = oscMatrix(rng);
}

// The refresh's tests: now and then an input's record put in the first four parts' tables at its kind and OSC, half
// the time with one of the bytes the refresh compares moved
void sameRecordsDrawn(OscSides& x, std::mt19937& rng) {
    constexpr uint32_t kCompared[] = { 1, 0x20, 0x3c, 0x5c, 0x59 };   // [FW] the type, waveform and PCM wave's words
    constexpr uint32_t kFewParts = 4;
    for (const Vc2SetOscInput& w : x.port.items) {
        if (rng() % 4) continue;
        std::array<uint8_t, kRecordBytes> r = w.record;
        if (rng() % 2) {
            const uint32_t at = kCompared[rng() % std::size(kCompared)];
            r[at] = (uint8_t)(r[at] ^ (1 + rng() % 255));
        }
        const uint32_t slot = kHeaderBytes + kKindRow * w.kind + kRecordBytes * w.osc;
        for (uint32_t p = 0; p < kFewParts; ++p) x.s.fw.cpu.write(kTableAt + kPartRow * p + slot, r.data(), sizeof r);
    }
}

void oscDraw(OscSides& x, std::mt19937& rng, bool all) {
    setsDraw(x.s, rng, all);
    tableDrawn(x.s.fw, rng, all);
    aroundDrawn(x.s.fw, rng, all);
    if (all || rng() % 8 == 0) setsClockDrawn(x.s.fw, rng);
    for (Vc2SetOscInput& w : x.port.items)
        if (all || rng() % 8 == 0) inputDrawn(w, rng);
    sameRecordsDrawn(x, rng);
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
    setsMakeEvent<OscSides, anyOscBlock, 0x8939fe00, 1, Blocks::kPerVoice, oscMatrix, oscKind, oscCall>,
    setsMakeEvent<OscSides, anyOscBlock, 0x8939fe00, 1, Blocks::kPerVoice, oscMatrix, oscKind, oscCall>,
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
    for (uint32_t k = 0; k < 4; ++k) s.fw.cpu.hook(kGetters[k], [k](Sh4& c) { c.ret(kGroupsAt + kGroupBytes * k); });
    s.fw.cpu.hook(kPitchSource, [](Sh4& c) {
        c.fb[0][0] = c.rd32(kSourceAt);
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
