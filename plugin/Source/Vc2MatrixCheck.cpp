// xt_vc2cosm's third part (v243): rom.4's Matrix core, the refresh (0x893a2230), every group's reader and the getter
// (0x893a2350), against Vc2MatrixCore, vc2MatrixRead and vc2MatrixSum on M in the interpreter's RAM and the port's
// words; from v245 on rom.4's own sources (0x893a1de0…), nothing hooked, the port's Vc2MatrixSourceRoutines on the
// same state: the channel's at M's +0x168, the seven assignments (0x89997398 + 0x848…), the part table (0x8993d040) by
// M's +0x16c; and the choice (0x893a1c70) an event of its own.  Each case M and the state drawn, then 60 events: a
// refresh, a group's reader, the getter on a destination, the choice, or M or the state drawn again; after each M and
// the answer (a reader's sums' place, the getter's sum, the choice's routine), word for word.  What it shares with the
// set-up's part in Vc2MatrixCheck.h (v247).  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2MatrixCheck.h"

namespace {
constexpr uint32_t kRefresh = 0x893a2230, kGetter = 0x893a2350, kChoice = 0x893a1c70;   // [FW]
// Each group's reader, in vc2MatrixGroups()' order (the plugin's from v343); the assignments the sources read
constexpr const auto& kReaders = kVc2MatrixReaders;
constexpr uint32_t kAssigns = 0x89997398 + 0x848;   // [FW]
constexpr int kParts = 4, kEvents = 60;

struct MatrixCounts {
    long events = 0, refreshes = 0, reads = 0, sums = 0, choices = 0, draws = 0, moved = 0;
};

struct MatrixRig {
    explicit MatrixRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    Vc2MatrixWords m {};
    Vc2MatrixWorld world { kParts };
    int16_t part = 0;
    Vc2MatrixSourceState state { &m };
    Vc2MatrixSourceRoutines sources { state };
    Vc2MatrixCore core { sources };
    MatrixCounts n;
    // vc2CosmPart's: rom.4's side hooked, a case's start, an event, the counts said
    void hook();
    void start(std::mt19937& rng);
    std::string event(std::mt19937& rng);
    void say(const Vc2Run& run) const;
};

// The sources' state drawn: the world (the bend at an edge of a slot's limit), the assignments (none, 32, a controller,
// any) and the part
void drawSourceState(MatrixRig& r, std::mt19937& rng) {
    r.world.drawChannel(rng, vc2MatrixLimit(r.m.slots[rng() % Vc2MatrixWords::kSlots].dest));
    for (uint8_t& a : r.state.assigns) {
        const std::array<uint8_t, 4> picks { 0, 32, (uint8_t)(1 + rng() % 95), (uint8_t)rng() };
        a = picks[rng() % picks.size()];
    }
    r.world.drawParts(rng);
    r.part = (int16_t)(rng() % kParts);
    r.state.partByte = r.world.partBytes[(size_t)r.part];
}

// A source parameter for the choice: one of its fourteen cases (96…109), a controller's number, an edge (0, 1, 95,
// 96, 110, 127), a byte or any word, half of them negative
int32_t drawSourceParam(std::mt19937& rng) {
    constexpr int32_t kCases = 96, kCaseCount = 14;
    constexpr std::array<int32_t, 6> edges { 0, 1, 95, 96, 110, 127 };
    const std::array<int32_t, 4> picks { kCases + (int32_t)(rng() % kCaseCount), (int32_t)(rng() % 128),
                                         edges[rng() % edges.size()],
                                         rng() % 2 ? (int32_t)(rng() % 256) : (int32_t)rng() };
    return picks[rng() % picks.size()];
}

// M, its two links and the sources' state into rom.4's memory
void writeAll(MatrixRig& r) {
    Sh4& cpu = r.fw.cpu;
    cpu.write(kVc2MatrixAt, &r.m, sizeof r.m);
    cpu.write(kVc2MatrixAt + kVc2MatrixLinks, &kVc2ChannelAt, 4);
    cpu.write(kVc2MatrixAt + kVc2MatrixLinks + 4, &r.part, 2);
    cpu.write(kAssigns, r.state.assigns.data(), r.state.assigns.size());
    r.world.write(cpu);
}

std::string matrixDifference(MatrixRig& r) {
    return vc2RegionsDifference(r.fw, { { kVc2MatrixAt, &r.m, sizeof r.m, "the Matrix object" } });
}

// A routine on both, rom.4's `routine` on `args` against the port's answer `mine`, worked out first: M and the answers
std::string answerOn(MatrixRig& r, uint32_t routine, std::initializer_list<uint32_t> args, uint32_t mine,
                     const std::string& what) {
    uint32_t r0 = 0;
    if (!r.fw.call(routine, args, &r0)) return what + " faults: " + r.fw.cpu.error();
    const std::string differs = matrixDifference(r);
    return differs.empty() && r0 != mine ? what : differs;
}

std::string matrixEvent(MatrixRig& r, std::mt19937& rng) {
    const int pick = (int)(rng() % 12);
    ++r.n.events;
    if (pick < 2) {
        ++r.n.draws;
        if (pick == 0) vc2DrawMatrixCore(r.m, rng);
        else drawSourceState(r, rng);
        return "";
    }
    writeAll(r);
    if (pick < 5) {
        ++r.n.refreshes;
        const Vc2MatrixWords before = r.m;
        const std::string what = vc2BothOn(r.fw, kRefresh, { kVc2MatrixAt }, "the refresh",
                                           [&] { r.core.refresh(r.m); }, [&] { return matrixDifference(r); });
        r.n.moved += std::memcmp(before.dirty, r.m.dirty, sizeof before.dirty) != 0;
        return what;
    }
    if (pick < 8) {
        ++r.n.reads;
        const size_t g = rng() % kReaders.size();
        if (vc2MatrixGroups().size() != kReaders.size()) return "the port's groups";
        const int16_t* sums = vc2MatrixRead(r.m, vc2MatrixGroups()[g], r.core);
        const auto at = (uint32_t)((const uint8_t*)sums - (const uint8_t*)&r.m);
        return answerOn(r, kReaders[g], { kVc2MatrixAt }, kVc2MatrixAt + at,
                        "group " + std::to_string(g) + "'s sums' place");
    }
    if (pick < 10) {
        ++r.n.sums;
        const int dest = (int)(rng() % Vc2MatrixTables::kDestinations);
        return answerOn(r, kGetter, { kVc2MatrixAt, (uint32_t)dest }, (uint32_t)vc2MatrixSum(r.m, dest),
                        "the sum of " + std::to_string(dest));
    }
    ++r.n.choices;
    const int32_t source = drawSourceParam(rng);
    return answerOn(r, kChoice, { (uint32_t)source }, vc2MatrixSourceRoutine(source),
                    "the choice of " + std::to_string(source));
}

// rom.4's side: the refresh and its sources run (the frame's part hooked the refresh)
void MatrixRig::hook() {
    fw.cpu.unhook(kRefresh);
    state.channel = &world.channel;
}

void MatrixRig::start(std::mt19937& rng) {
    vc2CosmNoise(m, rng);
    vc2DrawMatrixCore(m, rng);
    drawSourceState(*this, rng);
}

std::string MatrixRig::event(std::mt19937& rng) { return matrixEvent(*this, rng); }

void MatrixRig::say(const Vc2Run& run) const {
    std::printf("matrix: %d cases, %ld events: %ld refreshes (%ld setting the dirty bits), %ld reads, %ld sums, %ld "
                "choices, %ld draws\n", run.cases, n.events, n.refreshes, n.moved, n.reads, n.sums, n.choices,
                n.draws);
}
} // namespace

long vc2MatrixCases(Vc2Fw& fw, const Vc2Run& run) { return vc2CosmPart<MatrixRig>(fw, run, 2, kEvents); }
#endif
