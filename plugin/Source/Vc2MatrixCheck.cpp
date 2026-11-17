// xt_vc2cosm's third part (v243): rom.4's Matrix core, the refresh (0x893a2230), every group's reader and the getter
// (0x893a2350), against Vc2MatrixCore, vc2MatrixRead and vc2MatrixSum on M in the interpreter's RAM and the port's
// words.  Only the sources' routines are hooked: four stubs in RAM, M's routine words drawn among them, each call
// logging its stub and byte and answering the next of the event's drawn values, the port's Vc2MatrixSources the same.
// Each case M drawn whole, then 60 events: a refresh, a group's reader, the getter on a destination, or M's slots, uses,
// dirty bits and terms drawn again; after each M, both logs and the answer (a reader's sums' place, the getter's sum),
// word for word.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2CosmCheck.h"
#include "Vc2Matrix.h"

namespace {
constexpr uint32_t kRefresh = 0x893a2230, kGetter = 0x893a2350;   // [FW]
// [FW] each group's reader, in vc2MatrixGroups()' order
constexpr std::array<uint32_t, 10> kReaders { 0x893a23b0, 0x893a2440, 0x893a24d0, 0x893a2550, 0x893a26b0, 0x893a2750,
                                              0x893a28b0, 0x893a2960, 0x893a2aa0, 0x893a2bf0 };
constexpr uint32_t kMatrixAt = 0x8C070000, kStubsAt = 0x8C071000, kStubStride = 0x10;   // in Vc2Fw's RAM
constexpr int kStubs = 4, kEvents = 60;
constexpr int16_t kUnityDepth = 8192;   // a depth whose term is the value: 8192 × v >> 13

struct MatrixCall {
    uint32_t routine;
    uint8_t byte;
    bool operator==(const MatrixCall&) const = default;
};

struct MatrixCounts {
    long events = 0, refreshes = 0, reads = 0, sums = 0, draws = 0, calls = 0, moved = 0;
};

// Both sides' sources: each call logged and answered by the next of the event's drawn values
struct MatrixSources final : Vc2MatrixSources {
    std::vector<int32_t> answers = std::vector<int32_t>(Vc2MatrixCore::kSources);
    size_t next = 0;
    std::vector<MatrixCall> log;
    int32_t value(uint32_t routine, uint8_t byte) override {
        log.push_back({ routine, byte });
        return next < answers.size() ? answers[next++] : 0;
    }
};

struct MatrixRig {
    explicit MatrixRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    Vc2MatrixWords m {};
    MatrixSources rom, port;
    Vc2MatrixCore core { port };
    MatrixCounts n;
    // vc2CosmPart's: rom.4's side hooked, a case's start, an event, the counts said
    void hook();
    void start(std::mt19937& rng);
    std::string event(std::mt19937& rng);
    void say(const Vc2Run& run) const;
};

// An answer for source k: its last value (now and then with other high bits), 0, small, a 16-bit end, any 32 bits, or
// at an edge of its first slot's limit (with a unity depth, 8192, the term is the value: the holds' edges)
int32_t drawAnswer(const Vc2MatrixWords& m, int k, std::mt19937& rng) {
    const int32_t most = vc2MatrixTables().limits[m.slots[2 * k].dest];
    const std::array<int32_t, 5> edges { most, most + 1, -most, -most + 1, -most - 1 };
    const std::array<int32_t, 8> picks { m.last[k], m.last[k], (int32_t)(m.last[k] | (rng() << 16)), 0,
                                         (int)(rng() % 401) - 200, rng() % 2 ? 32767 : -32768, (int32_t)rng(),
                                         edges[rng() % edges.size()] };
    return picks[rng() % picks.size()];
}

// M's words drawn: the slots (destinations within the limits' table, depths), the uses (now and then none of a
// group's), the dirty bits and the terms; the routines always the stubs
void drawMatrixCore(Vc2MatrixWords& m, std::mt19937& rng) {
    for (Vc2MatrixSlot& s : m.slots) {
        s.dest = (uint8_t)(rng() % 5 == 0 ? 0 : rng() % Vc2MatrixTables::kDestinations);
        const std::array<int16_t, 4> depths { (int16_t)((int)(rng() % 255) - 127), (int16_t)((int)(rng() % 255) - 127),
                                              (int16_t)rng(), kUnityDepth };
        s.depth = depths[rng() % depths.size()];
    }
    for (uint16_t& u : m.uses) u = rng() % 4 == 0 ? 0 : (uint16_t)rng();
    for (uint16_t& d : m.dirty) d = rng() % 3 == 0 ? 0 : (uint16_t)rng();
    for (int16_t& t : m.terms) t = rng() % 6 == 0 ? (int16_t)rng() : (int16_t)((int)(rng() % 255) - 127);
    for (uint32_t& r : m.routines) r = kStubsAt + kStubStride * (uint32_t)(rng() % kStubs);
}

std::string matrixDifference(MatrixRig& r) {
    const std::string what = vc2RegionsDifference(r.fw, { { kMatrixAt, &r.m, sizeof r.m, "the Matrix object" } });
    if (!what.empty()) return what;
    return vc2CallsDifference(r.rom.log, r.port.log, [](const MatrixCall& c) {
        return "source " + std::to_string(c.routine) + " on byte " + std::to_string(c.byte);
    });
}

// A group's reader on both: M, the calls and where its sums are
std::string readOn(MatrixRig& r, size_t g) {
    uint32_t r0 = 0;
    if (!r.fw.call(kReaders[g], { kMatrixAt }, &r0)) return "a reader faults: " + r.fw.cpu.error();
    if (vc2MatrixGroups().size() != kReaders.size()) return "the port has " + std::to_string(vc2MatrixGroups().size()) +
                                                            " groups";
    const int16_t* sums = vc2MatrixRead(r.m, vc2MatrixGroups()[g], r.core);
    const std::string what = matrixDifference(r);
    const auto at = (uint32_t)((const uint8_t*)sums - (const uint8_t*)&r.m);
    return what.empty() && r0 - kMatrixAt != at ? "group " + std::to_string(g) + "'s sums' place" : what;
}

// The getter on both: M, the calls and the sum
std::string sumOn(MatrixRig& r, int dest) {
    uint32_t r0 = 0;
    if (!r.fw.call(kGetter, { kMatrixAt, (uint32_t)dest }, &r0)) return "the getter faults: " + r.fw.cpu.error();
    const std::string what = matrixDifference(r);
    return what.empty() && (int32_t)r0 != vc2MatrixSum(r.m, dest) ? "the sum of " + std::to_string(dest) : what;
}

std::string matrixEvent(MatrixRig& r, std::mt19937& rng) {
    for (int k = 0; k < Vc2MatrixCore::kSources; ++k) r.port.answers[(size_t)k] = drawAnswer(r.m, k, rng);
    r.rom.answers = r.port.answers;
    r.rom.next = r.port.next = 0;
    r.rom.log.clear();
    r.port.log.clear();
    const int pick = (int)(rng() % 10);
    ++r.n.events;
    if (pick == 0) {
        ++r.n.draws;
        drawMatrixCore(r.m, rng);
        return "";
    }
    r.fw.cpu.write(kMatrixAt, &r.m, sizeof r.m);
    std::string what;
    if (pick < 4) {
        ++r.n.refreshes;
        const Vc2MatrixWords before = r.m;
        what = vc2BothOn(r.fw, kRefresh, { kMatrixAt }, "the refresh", [&] { r.core.refresh(r.m); },
                         [&] { return matrixDifference(r); });
        r.n.moved += std::memcmp(before.dirty, r.m.dirty, sizeof before.dirty) != 0;
    } else if (pick < 8) {
        ++r.n.reads;
        what = readOn(r, rng() % kReaders.size());
    } else {
        ++r.n.sums;
        what = sumOn(r, (int)(rng() % Vc2MatrixTables::kDestinations));
    }
    r.n.calls += (long)r.port.log.size();
    return what;
}

// rom.4's side: the refresh runs (the frame's part hooked it), each stub logs its call and answers
void MatrixRig::hook() {
    fw.cpu.unhook(kRefresh);
    for (int s = 0; s < kStubs; ++s) {
        const uint32_t at = kStubsAt + kStubStride * (uint32_t)s;
        fw.cpu.hook(at, [this, at](Sh4& cpu) { cpu.ret((uint32_t)rom.value(at, (uint8_t)cpu.r[5])); });
    }
}

void MatrixRig::start(std::mt19937& rng) {
    vc2CosmNoise(m, rng);
    drawMatrixCore(m, rng);
}

std::string MatrixRig::event(std::mt19937& rng) { return matrixEvent(*this, rng); }

void MatrixRig::say(const Vc2Run& run) const {
    std::printf("matrix: %d cases, %ld events: %ld refreshes (%ld setting the dirty bits), %ld reads, %ld sums, %ld "
                "draws; %ld sources' calls\n", run.cases, n.events, n.refreshes, n.moved, n.reads, n.sums, n.draws,
                n.calls);
}
} // namespace

long vc2MatrixCases(Vc2Fw& fw, const Vc2Run& run) { return vc2CosmPart<MatrixRig>(fw, run, 2, kEvents); }
#endif
