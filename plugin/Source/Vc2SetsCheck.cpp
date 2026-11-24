// xt_vc2sets (v280): the VC-2 part's conversion sets C and the notes' Matrix objects M, Vc2Sets and Vc2NoteMatrices,
// against rom.4's own routines on its own C, M, notes W, Matrix settings G and the part's M in .bss.  Every routine of
// C's children (the OSC blocks', common's, mix's, envelopes', LFOs', COSM inputs' and TVA's allocators, constructors,
// ends, conversions, updates, copies, releases and refreshes) is hooked to log its arguments (a call struct read
// through its pointer, the constructor's stacked words) and answer from a generator per side seeded alike before each
// event (an allocator an object or none, a refresh a word).  M's routines, the head 0x893a2eb0 and the block move run
// on both.  The voices' words a call points at, the key tables, the channels, the parts and the part's bytes by kind
// are drawn in rom.4's memory, which the port reads too.  Each case noise, then 400 events (Vc2SetsEvents.cpp); before
// each the draws (Vc2SetsDraws.cpp); after each C, M, W, the part's M, the calls and the answer.  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2CarrierTables.h"
#include "Vc2CosmTables.h"
#include "Vc2OscConvertTables.h"
#include "Vc2LibMathTables.h"
#include "Vc2SetEnvelopeTables.h"
#include "Vc2SetsChildCheck.h"
#include <algorithm>
#include <cstring>

namespace {
using Args = std::array<uint32_t, 8>;
constexpr int kSteps = 400;
constexpr uint32_t kEnded = 3;   // [FW] what every end is given beside its object

// A call struct's words through its pointer, after the object; `extra` past them
Args structArgs(Sh4& c, uint32_t extra) {
    const uint32_t s = c.r[5];
    return { c.r[4], c.rd32(s), c.rd32(s + 4), c.rd32(s + 8), extra };
}

// [FW] what rom.4's own read of what C gives them, the check's and not the port's: the allocators that take the index,
// and each constructor's stacked words past M (none and no M the common's; the kind; the kind and the index)
constexpr uint32_t kIndexed[] = { 0x8939fdd0, 0x8939a620, 0x8939b690, 0x89399ae0 };
constexpr std::pair<uint32_t, uint32_t> kStacked[] = {
    { 0x893a1960, 0 }, { 0x8939fe00, 2 }, { 0x8939bdb0, 1 }, { 0x8939a690, 2 }, { 0x8939b6c0, 2 }, { 0x89399b10, 2 },
    { 0x893a67f0, 1 },
};

bool takesIndex(uint32_t allocator) { return std::ranges::find(kIndexed, allocator) != std::end(kIndexed); }

uint32_t stackedOf(uint32_t constructor) {
    const auto at = std::ranges::find(kStacked, constructor, &std::pair<uint32_t, uint32_t>::first);
    return at == std::end(kStacked) ? 0 : at->second;
}

Args constructArgs(Sh4& c, uint32_t routine) {
    const uint32_t sp = c.r[15], s = c.r[5], n = stackedOf(routine);
    return { c.r[4], c.rd32(s), c.rd32(s + 4), c.rd32(s + 8), c.r[6], n != 0 ? c.r[7] : 0, n >= 1 ? c.rd32(sp) : 0,
             n >= 2 ? c.rd32(sp + 4) : 0 };
}

// rom.4's arguments by method
using RomArgs = Args (*)(Sh4&, uint32_t);
constexpr RomArgs kRomArgs[kSetMethods] = {
    [](Sh4& c, uint32_t routine) -> Args { return { c.r[4], c.r[5], takesIndex(routine) ? c.r[6] : 0 }; },
    constructArgs,
    [](Sh4& c, uint32_t) -> Args { return { c.r[4], c.r[5] }; },
    [](Sh4& c, uint32_t) { return structArgs(c, c.r[6]); },
    [](Sh4& c, uint32_t) { return structArgs(c, 0); },
    [](Sh4& c, uint32_t) -> Args { return { c.r[4], c.r[5] }; },
    [](Sh4& c, uint32_t) -> Args { return { c.r[4], c.r[5], c.r[6] }; },
    [](Sh4& c, uint32_t) { return structArgs(c, 0); },
    [](Sh4& c, uint32_t) -> Args { return { c.r[4] }; },
};

uint32_t methodRoutine(const Vc2SetChild& r, int32_t method) {
    const uint32_t routines[kSetMethods] = { r.allocate, r.construct, r.end, r.convert, r.update, r.copy, r.release,
                                             r.refresh, 0 };
    return routines[method];
}

bool answering(int32_t method) { return method == kSetAllocate || method == kSetRefresh; }

// rom.4's side: every child's routine logged and answered
void setsInstallRom(SetSides& s) {
    for (const Vc2SetChild& row : Vc2Sets::children())
        for (int32_t m = 0; m < kSetMethods; ++m) {
            const uint32_t at = methodRoutine(row, m);
            if (at == 0) continue;
            s.fw.cpu.hook(at, [&s, m, at](Sh4& c) {
                s.rom.push_back({ m, at, kRomArgs[m](c, at) });
                c.ret(answering(m) ? setAnswer(m, s.romAnswers) : 0);
            });
        }
}

Args portStruct(uint32_t object, const Vc2SetCall& call, uint32_t extra) {
    return { object, call.channelAt, call.byteAt, call.word, extra };
}

std::string saidCall(const SetCall& c) {
    char t[160];
    std::snprintf(t, sizeof t, "%d at %08x (%08x %08x %08x %08x %08x %08x %08x %08x)", c.kind, c.routine, c.args[0],
                  c.args[1], c.args[2], c.args[3], c.args[4], c.args[5], c.args[6], c.args[7]);
    return t;
}

std::string setsDifference(SetSides& s) {
    const std::string words = vc2RegionsDifference(s.fw, {
        { Vc2Sets::kAt, &s.port.items, sizeof s.port.items, "C" },
        { Vc2NoteMatrices::kAt, &s.port.matrices.items, sizeof s.port.matrices.items, "M" },
        { Vc2Notes::kNotesAt, &s.notes.notes, sizeof s.notes.notes, "W" },
        { kSetsMatrixAt, &s.partM, sizeof s.partM, "the part's M" },
        { kSetsMatrixAt + sizeof s.partM, &s.partLinks, sizeof s.partLinks, "the part's M's links" },
    });
    return words.empty() ? vc2CallsDifference(s.rom, s.mine, saidCall) : words;
}

std::string setsCase(SetSides& s, std::mt19937& rng) {
    setsNoise(s.port.items, rng);
    setsNoise(s.port.matrices.items, rng);
    setsNoise(s.notes.notes, rng);
    std::string what;
    const std::span<const SetEvent> events = setEvents();
    for (int k = 0; k < kSteps && what.empty(); ++k, ++s.n.events) {
        setsDraw(s, rng, k == 0);
        what = events[rng() % events.size()](s, rng);
        for (const SetCall& c : s.mine) ++s.n.calls[(size_t)c.kind];
    }
    return what;
}

void saySetCounts(const Vc2Run& run, const SetCounts& n, long different) {
    const auto& e = n.byEvent;
    const auto& c = n.calls;
    std::printf("sets: %d cases, %ld events: %ld placements, %ld makes (%ld with no common), %ld ends, %ld "
                "conversions, %ld releases, %ld updates, %ld copies, %ld refreshes; M: %ld placements, %ld "
                "constructors, %ld updates\n", run.cases, n.events, e[0], e[1], n.noCommon, e[2], e[3], e[4], e[5],
                e[6], e[7], e[8], e[9], e[10]);
    std::printf("children: %ld allocations, %ld constructors, %ld ends, %ld conversions, %ld updates, %ld copies, "
                "%ld releases, %ld refreshes\n", c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7]);
    std::printf("xt_vc2sets: %ld different\n", different);
}
} // namespace

uint32_t setAnswer(int32_t kind, std::mt19937& rng) {
    if (kind == kSetRefresh) {
        const uint32_t any = (uint32_t)rng();
        const uint32_t words[3] = { 0, 1, any };
        return words[rng() % 3];
    }
    return rng() % 8 == 0 ? 0 : kSetsChildrenAt + kSetsChildBytes * (uint32_t)(rng() % kSetsChildren);
}

SetSides::SetSides(Vc2Fw& rig) : fw(rig), parts(rig, portAnswers, mine) { setsInstallRom(*this); }

void SetPortParts::logged(int32_t kind, uint32_t routine, std::array<uint32_t, 8> args) {
    log_.push_back({ kind, routine, args });
}

uint32_t SetPortParts::allocate(const Vc2SetChild& c, uint32_t number, uint32_t index) {
    logged(kSetAllocate, c.allocate, { c.bytes, number, takesIndex(c.allocate) ? index : 0 });
    return setAnswer(kSetAllocate, answers_);
}

void SetPortParts::construct(const Vc2SetChild& c, const Vc2SetChildCall& k) {
    const uint32_t n = stackedOf(c.construct);
    logged(kSetConstruct, c.construct, { k.object, k.call.channelAt, k.call.byteAt, k.call.word, k.noteAt,
                                         n != 0 ? k.matrix : 0, n >= 1 ? k.kind : 0, n >= 2 ? k.index : 0 });
}

void SetPortParts::end(const Vc2SetChild& c, uint32_t object) { logged(kSetEnd, c.end, { object, kEnded }); }

void SetPortParts::convert(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call, uint32_t noteAt) {
    logged(kSetConvert, c.convert, portStruct(object, call, noteAt));
}

void SetPortParts::update(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) {
    logged(kSetUpdate, c.update, portStruct(object, call, 0));
}

void SetPortParts::copy(const Vc2SetChild& c, uint32_t object, uint32_t noteAt) {
    logged(kSetCopy, c.copy, { object, noteAt });
}

void SetPortParts::release(const Vc2SetChild& c, uint32_t object, uint32_t velocity, uint32_t noteAt) {
    logged(kSetRelease, c.release, { object, velocity, noteAt });
}

uint32_t SetPortParts::refresh(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) {
    logged(kSetRefresh, c.refresh, portStruct(object, call, 0));
    return setAnswer(kSetRefresh, answers_);
}

int16_t SetPortParts::matrixSum(uint32_t reader, uint32_t matrix, uint32_t offset) {
    logged(kSetMatrixSum, reader, { matrix });
    return (int16_t)fw_.cpu.rd16(kSetsSumsAt + offset);
}

long vc2SetsFurther(SetSides& s, const Vc2Run& run) {
    const long children = vc2SetCommonPart(s, run) + vc2SetEnvelopePart(s, run) + vc2SetLfoPart(s, run);
    const long kinds = vc2SetMixPart(s, run) + vc2SetTvaPart(s, run) + vc2SetCosmInputPart(s, run);
    return children + kinds + vc2SetOscBlockPart(s, run);
}

uint32_t* setsPortWords(SetSides& s, uint32_t at) {
    auto* notes = reinterpret_cast<uint8_t*>(s.notes.notes.data());
    return reinterpret_cast<uint32_t*>(notes + (at - Vc2Notes::kNotesAt));
}

Vc2SetCall setsCallAt(SetSides& s, uint32_t at) {
    Vc2SetCall call;
    s.fw.cpu.read(at, &call, sizeof call);
    return call;
}

std::string setsChildOnBoth(SetSides& s, std::span<const Vc2Region> pools, const Vc2CheckRoutine& routine,
                            std::initializer_list<uint32_t> args, const std::function<uint32_t()>& mine) {
    for (const Vc2Region& pool : pools)
        if (pool.bytes != 0) s.fw.cpu.write(pool.at, pool.mine, pool.bytes);
    s.fw.cpu.write(Vc2Notes::kNotesAt, &s.notes.notes, sizeof s.notes.notes);
    s.rom.clear();
    s.mine.clear();
    const std::string what = vc2BothAnswer(s.fw, routine, args, mine, [&] {
        const Vc2Region notes { Vc2Notes::kNotesAt, &s.notes.notes, sizeof s.notes.notes, "W" };
        std::string words = vc2RegionsDifference(s.fw, { pools[0], notes });
        for (size_t k = 1; k < pools.size() && words.empty(); ++k)
            if (pools[k].bytes != 0) words = vc2RegionsDifference(s.fw, { pools[k] });
        return words.empty() ? vc2CallsDifference(s.rom, s.mine, saidCall) : words;
    });
    return what.empty() ? what : std::string(routine.name) + ": " + what;
}

std::string setsOnBoth(SetSides& s, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                       const std::function<uint32_t()>& mine) {
    s.fw.cpu.write(Vc2Sets::kAt, &s.port.items, sizeof s.port.items);
    s.fw.cpu.write(Vc2NoteMatrices::kAt, &s.port.matrices.items, sizeof s.port.matrices.items);
    s.fw.cpu.write(Vc2Notes::kNotesAt, &s.notes.notes, sizeof s.notes.notes);
    s.fw.cpu.write(kSetsSettingsAt, &s.g, sizeof s.g);
    s.fw.cpu.write(kSetsMatrixAt, &s.partM, sizeof s.partM);
    s.fw.cpu.write(kSetsMatrixAt + sizeof s.partM, &s.partLinks, sizeof s.partLinks);
    s.rom.clear();
    s.mine.clear();
    s.romAnswers.seed(s.seed);
    s.portAnswers.seed(s.seed);
    const std::string what = vc2BothAnswer(s.fw, routine, args, mine, [&] { return setsDifference(s); });
    return what.empty() ? what : std::string(routine.name) + ": " + what;
}

int main(int argc, char** argv) {
    constexpr auto load = vc2LoadAll<vc2LoadMatrix, vc2LoadLibMath, vc2LoadSetEnvelopes, vc2LoadCosm, vc2LoadOscConvert,
                                     vc2LoadCarrier>;
    const Vc2CheckStart start { "xt_vc2sets", load, { "XT_VC2SETS_FIRST", "XT_VC2SETS_STOP" } };
    return vc2CheckMain<SetSides>(argc, argv, start, setsCase, saySetCounts, vc2SetsFurther);
}
#endif
