// xt_vc2cosm's second part (v235): the COSM converters' frame, rom.4's dispatcher (0x89399920) against
// vc2CosmConvert on the same input and block, with the note's Matrix object M in the interpreter's RAM and its port's
// words beside it.  The Matrix readers and getter run on both sides; the refresh (0x893a2230) is hooked to log and to
// do what a refresh does, drawn the same on both sides (one time in two a source moved: new terms, every group's dirty
// bits its uses); the velocity curve, powf and logf are rom.4's own on both (RomShared); the converters the port has
// run on both (vc2CosmPortedConvert: from v241 every one), any the check's list has not are hooked to log their type,
// the port's Vc2CosmTypes the same.  Each case M drawn, then 60 events: an input drawn (Vc2CosmFrameDraws.cpp: the
// switch now and then off, the type mostly one the port has, else any, past 16 too; the curves, sensitivities, key,
// velocity and the type's parameters at and past their ends), M's slots and terms now and then drawn again; after each
// the input, the block, M and the calls, word for word.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2CosmCheck.h"
#include "Vc2CosmConvert.h"
#include "Vc2CosmConverters.h"
#include "Vc2CosmTables.h"
#include "Vc2Matrix.h"
#include "Vc2MatrixTables.h"
#include "Vc2RomShared.h"
#include <array>
#include <numeric>

namespace {
constexpr uint32_t kDispatch = 0x89399920, kRefresh = 0x893a2230;   // [FW]
// [FW] the dispatcher's converters by type, 1…16 (4's inline); the readers, which the Growl's part hooks
constexpr std::array<uint32_t, 17> kConverters { 0, 0x89397980, 0x89397cf0, 0x89397eb0, 0, 0x89398080, 0x89398260,
    0x893984e0, 0x89398780, 0x893988a0, 0x89398ae0, 0x89398d40, 0x89398fd0, 0x893990d0, 0x893991d0, 0x893993f0,
    0x89399620 };
constexpr std::array<uint32_t, 2> kReaders { 0x893a2960, 0x893a2aa0 };
constexpr int kSpeaker = 4, kEvents = 60, kRefreshCall = 0;
constexpr uint32_t kInputAt = 0x8C060000, kBlockAt = 0x8C060100, kMatrixAt = 0x8C060200;   // in Vc2Fw's RAM

struct FrameCall {
    int32_t kind;   // 0 a refresh, else a hooked converter's type
    bool operator==(const FrameCall&) const = default;
};

struct FrameCounts {
    long events = 0, off = 0, refreshes = 0;
    std::array<long, 256> byType {};   // the events with the switch on, by the input's type
};

// What a refresh does, drawn: one time in two a source moved, so new terms (most within a destination's limit, some
// at 16 bits' ends) and every group's dirty bits its uses
void moveSources(Vc2MatrixWords& m, std::mt19937& rng) {
    if (rng() % 2) return;
    for (int16_t& t : m.terms) t = rng() % 6 == 0 ? (int16_t)rng() : (int16_t)((int)(rng() % 255) - 127);
    std::ranges::copy(m.uses, m.dirty);
}

// Both sides' refreshes: each logs its call and moves the sources by its own generator, both seeded the same
struct FrameRefresh final : Vc2MatrixRefresh {
    std::mt19937 rng;
    std::vector<FrameCall> log;
    void refresh(Vc2MatrixWords& m) override {
        log.push_back({ kRefreshCall });
        moveSources(m, rng);
    }
};

// The port's converters: those it has, on rom.4's own powf and logf (v239), the rest logged as rom.4's hooks log them
struct FrameTypes final : Vc2CosmTypes {
    Vc2CosmMatrix& matrix;
    const Vc2ToneShared& shared;
    std::vector<FrameCall>& log;
    FrameTypes(Vc2CosmMatrix& m, const Vc2ToneShared& s, std::vector<FrameCall>& l) : matrix(m), shared(s), log(l) {}
    void convert(int type, const Vc2CosmInput& in, uint32_t* block) override {
        if (!vc2CosmPortedConvert(type, in, block, matrix, shared)) log.push_back({ type });
    }
};

// M's words: noise, then the COSM groups' uses (now and then none), slots naming their destinations, terms
void drawMatrixWords(Vc2MatrixWords& m, std::mt19937& rng, bool all) {
    if (all) {
        std::array<uint32_t, sizeof m / 4> noise {};
        for (uint32_t& w : noise) w = (uint32_t)rng();
        std::memcpy(&m, noise.data(), sizeof m);
    }
    for (int g : { kVc2MatrixCosm1.index, kVc2MatrixCosm2.index })
        m.uses[g] = rng() % 6 == 0 ? (uint16_t)(rng() & ~0x7ffu) : (uint16_t)rng();
    for (Vc2MatrixSlot& s : m.slots)
        s.dest = (uint8_t)(rng() % 4 ? 46 + rng() % 22 : rng() % Vc2MatrixTables::kDestinations);
    moveSources(m, rng);
}

struct FrameRig {
    explicit FrameRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    Vc2MatrixWords m {};
    FrameRefresh rom, port;
    Vc2CosmMatrixReader reader { m, port };
    std::vector<FrameCall> hooked;   // rom.4's converters' hooks', then the port's types'
    std::vector<FrameCall> types;
    RomShared shared { fw };
    FrameTypes portTypes { reader, shared, types };
};

std::string frameDifference(FrameRig& r, const Vc2CosmInput& in, const std::array<uint32_t, kVc2CosmBlockWords>& b) {
    const std::string what = vc2RegionsDifference(r.fw, { { kInputAt, &in, sizeof in, "the input" },
        { kBlockAt, b.data(), sizeof b, "the block" }, { kMatrixAt, &r.m, sizeof r.m, "the Matrix object" } });
    if (!what.empty()) return what;
    const auto said = [](const FrameCall& c) {
        return c.kind == kRefreshCall ? std::string("the refresh") : "converter " + std::to_string(c.kind);
    };
    const std::string calls = vc2CallsDifference(r.rom.log, r.port.log, said);
    return calls.empty() ? vc2CallsDifference(r.hooked, r.types, said) : calls;
}

std::string frameEvent(FrameRig& r, std::mt19937& rng, FrameCounts& n) {
    Vc2CosmInput in {};
    vc2DrawCosmInput(in, rng, kMatrixAt);
    std::array<uint32_t, kVc2CosmBlockWords> block {};
    for (uint32_t& w : block) w = (uint32_t)rng();
    if (rng() % 4 == 0) drawMatrixWords(r.m, rng, false);
    const uint32_t seed = rng();
    r.rom.rng.seed(seed);
    r.port.rng.seed(seed);
    for (auto* log : { &r.rom.log, &r.port.log, &r.hooked, &r.types }) log->clear();
    r.fw.cpu.write(kInputAt, &in, sizeof in);
    r.fw.cpu.write(kBlockAt, block.data(), sizeof block);
    r.fw.cpu.write(kMatrixAt, &r.m, sizeof r.m);
    ++n.events;
    ++(in.on == 0 ? n.off : n.byType[in.type]);
    std::string what = vc2BothOn(r.fw, kDispatch, { kInputAt, kBlockAt }, "the dispatcher",
                                 [&] { vc2CosmConvert(in, block.data(), r.portTypes, r.shared); },
                                 [&] { return frameDifference(r, in, block); });
    n.refreshes += (long)r.port.log.size();
    if (what.empty() && r.shared.faulted) what = "the velocity curve faults: " + r.fw.cpu.error();
    return what;
}

// rom.4's side: the refresh moves M's sources in its RAM, the converters not ported log their type (by the check's own
// list, so a type the port lost or gained differs)
void hookFrame(FrameRig& r) {
    for (uint32_t reader : kReaders) r.fw.cpu.unhook(reader);
    r.fw.cpu.hook(kRefresh, [&r](Sh4& cpu) {
        Vc2MatrixWords m {};
        cpu.read(cpu.r[4], &m, sizeof m);
        r.rom.refresh(m);
        cpu.write(cpu.r[4], &m, sizeof m);
        cpu.ret(0);
    });
    for (size_t t = 1; t < kConverters.size(); ++t) {
        if (kConverters[t] == 0 || vc2CosmCheckPorted((int)t)) continue;
        r.fw.cpu.hook(kConverters[t], [&r, t](Sh4& cpu) {
            r.hooked.push_back({ (int32_t)t });
            cpu.ret(0);
        });
    }
}

void sayFrameCounts(const Vc2Run& run, const FrameCounts& n) {
    std::string ported;
    long types = 0, have = 0;
    for (size_t t = 1; t < kConverters.size(); ++t) {
        types += n.byType[t];
        if (!vc2CosmCheckPorted((int)t)) continue;
        have += n.byType[t];
        ported += (ported.empty() ? "" : ", ") + std::to_string(n.byType[t]) + " type " + std::to_string(t);
    }
    std::printf("frame: %d cases, %ld events: %s, %ld Speaker Simulator, %ld other types, %ld off, %ld past the types; "
                "%ld refreshes\n", run.cases, n.events, ported.c_str(), n.byType[kSpeaker],
                types - have - n.byType[kSpeaker], n.off, n.events - n.off - types, n.refreshes);
}
} // namespace

bool vc2LoadCosmAndMatrix(const char* path) { return vc2LoadCosm(path) && vc2LoadMatrix(path); }

long vc2CosmFrameCases(Vc2Fw& fw, const Vc2Run& run) {
    FrameRig r { fw };
    hookFrame(r);
    std::mt19937 rng((uint32_t)run.seed + 1);
    FrameCounts n;
    Vc2Tally tally { "XT_VC2COSM_FIRST", "XT_VC2COSM_STOP" };
    for (int c = 0; c < run.cases; ++c) {
        drawMatrixWords(r.m, rng, true);
        std::string what;
        for (int e = 0; e < kEvents && what.empty(); ++e) what = frameEvent(r, rng, n);
        if (vc2CaseOutcome(tally, what, c, n.events)) break;
    }
    sayFrameCounts(run, n);
    return tally.different;
}
#endif
