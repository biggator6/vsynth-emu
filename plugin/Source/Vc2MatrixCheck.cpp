// xt_vc2cosm's third part (v243): rom.4's Matrix core, the refresh (0x893a2230), every group's reader and the getter
// (0x893a2350), against Vc2MatrixCore, vc2MatrixRead and vc2MatrixSum on M in the interpreter's RAM and the port's
// words; from v245 on rom.4's own sources (0x893a1de0…), nothing hooked, the port's Vc2MatrixSourceRoutines on the
// same state: the channel's at M's +0x168, the seven assignments (0x89997398 + 0x848…), the part table (0x8993d040) by
// M's +0x16c; and the choice (0x893a1c70) an event of its own.  Each case M and the state drawn, then 60 events: a
// refresh, a group's reader, the getter on a destination, the choice, or M or the state drawn again; after each M and
// the answer (a reader's sums' place, the getter's sum, the choice's routine), word for word.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmCheck.h"
#include "Vc2MatrixSources.h"

namespace {
constexpr uint32_t kRefresh = 0x893a2230, kGetter = 0x893a2350, kChoice = 0x893a1c70;   // [FW]
// [FW] each group's reader, in vc2MatrixGroups()' order; the sources' routines; where the sources read
constexpr std::array<uint32_t, 10> kReaders { 0x893a23b0, 0x893a2440, 0x893a24d0, 0x893a2550, 0x893a26b0, 0x893a2750,
                                              0x893a28b0, 0x893a2960, 0x893a2aa0, 0x893a2bf0 };
constexpr std::array<uint32_t, 16> kSources { 0x893a1de0, 0x893a1df0, 0x893a1e10, 0x893a1e30, 0x893a1e60, 0x893a1ef0,
    0x893a1f30, 0x893a1f70, 0x893a1fb0, 0x893a1fd0, 0x893a1ff0, 0x893a2000, 0x893a2040, 0x893a2080, 0x893a2130,
    0x893a21f0 };
constexpr uint32_t kBend = 0x893a1e10, kAssigns = 0x89997398 + 0x848, kPartTable = 0x8993d040;   // [FW]
constexpr uint32_t kLinks = 0x168, kPartByte = 0x24;                 // [FW] M's channel and part words; a part's byte
// In Vc2Fw's RAM: M, the channel's state, the parts
constexpr uint32_t kMatrixAt = 0x8C070000, kChannelAt = 0x8C071000, kPartsAt = 0x8C072000, kPartStride = 0x40;
constexpr int kParts = 4, kEvents = 60;
constexpr int16_t kUnityDepth = 8192;   // a depth whose term is the value: 8192 × v >> 13

struct MatrixCounts {
    long events = 0, refreshes = 0, reads = 0, sums = 0, choices = 0, draws = 0, moved = 0;
};

struct MatrixRig {
    explicit MatrixRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    Vc2MatrixWords m {};
    Vc2MatrixChannel channel {};
    std::array<uint8_t, kParts> partBytes {};
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

// A byte as MIDI gives it, now and then an edge (0, the bipolar reading's centre and either side, 127) or any
uint8_t drawMidiByte(std::mt19937& rng) {
    constexpr std::array<uint8_t, 6> edges { 0, 1, 63, 64, 65, 127 };
    const uint32_t pick = rng() % 16;
    return (uint8_t)(pick < 12 ? rng() % 128 : pick < 15 ? edges[rng() % edges.size()] : rng());
}

// M's words drawn: its head (velocity, keys), the slots (destinations mostly within the limits' table, one in eight any
// byte, whose limit is the curve's word; depths mostly ±127, some any 16 bits, some the unity depth), the uses (now and
// then none of a group's), dirty bits and terms; the routines rom.4's sources, the bend often
void drawMatrixCore(Vc2MatrixWords& m, std::mt19937& rng) {
    for (uint8_t& h : m.head) h = drawMidiByte(rng);
    for (uint8_t& s : m.sources) s = drawMidiByte(rng);
    for (Vc2MatrixSlot& s : m.slots) {
        const uint32_t within = rng() % 5 == 0 ? 0 : rng() % Vc2MatrixTables::kDestinations;
        s.dest = (uint8_t)(rng() % 8 == 0 ? rng() : within);
        const std::array<int16_t, 4> depths { (int16_t)((int)(rng() % 255) - 127), (int16_t)((int)(rng() % 255) - 127),
                                              (int16_t)rng(), kUnityDepth };
        s.depth = depths[rng() % depths.size()];
    }
    for (uint16_t& u : m.uses) u = rng() % 4 == 0 ? 0 : (uint16_t)rng();
    for (uint16_t& d : m.dirty) d = rng() % 3 == 0 ? 0 : (uint16_t)rng();
    for (int16_t& t : m.terms) t = rng() % 6 == 0 ? (int16_t)rng() : (int16_t)((int)(rng() % 255) - 127);
    for (uint32_t& r : m.routines) r = rng() % 4 == 0 ? kBend : kSources[rng() % kSources.size()];
}

// The sources' state drawn: the channel (the bend any, centred or at an edge of a slot's limit; the aftertouches and
// controllers as MIDI gives them, now and then any), the assignments (none, 32, a controller, any) and the parts
void drawSourceState(MatrixRig& r, std::mt19937& rng) {
    const int32_t most = vc2MatrixLimit(r.m.slots[rng() % Vc2MatrixWords::kSlots].dest);
    const std::array<int32_t, 6> edges { most, most + 1, -most, -most + 1, -most - 1, 0 };
    r.channel.bend = (uint16_t)(rng() % 2 ? 8192 + edges[rng() % edges.size()] : rng());
    r.channel.aftertouch = drawMidiByte(rng);
    for (uint8_t& p : r.channel.polyAftertouch) p = drawMidiByte(rng);
    for (uint8_t& c : r.channel.controllers) c = drawMidiByte(rng);
    for (uint8_t& a : r.state.assigns) {
        const std::array<uint8_t, 4> picks { 0, 32, (uint8_t)(1 + rng() % 95), (uint8_t)rng() };
        a = picks[rng() % picks.size()];
    }
    for (uint8_t& b : r.partBytes) b = drawMidiByte(rng);
    r.part = (int16_t)(rng() % kParts);
    r.state.partByte = r.partBytes[(size_t)r.part];
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
    cpu.write(kMatrixAt, &r.m, sizeof r.m);
    cpu.write(kMatrixAt + kLinks, &kChannelAt, 4);
    cpu.write(kMatrixAt + kLinks + 4, &r.part, 2);
    cpu.write(kChannelAt, &r.channel, sizeof r.channel);
    cpu.write(kAssigns, r.state.assigns.data(), r.state.assigns.size());
    for (uint32_t p = 0; p < kParts; ++p) {
        const uint32_t at = kPartsAt + kPartStride * p;
        cpu.write(kPartTable + 4 * p, &at, 4);
        cpu.write(at + kPartByte, &r.partBytes[p], 1);
    }
}

std::string matrixDifference(MatrixRig& r) {
    return vc2RegionsDifference(r.fw, { { kMatrixAt, &r.m, sizeof r.m, "the Matrix object" } });
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
        if (pick == 0) drawMatrixCore(r.m, rng);
        else drawSourceState(r, rng);
        return "";
    }
    writeAll(r);
    if (pick < 5) {
        ++r.n.refreshes;
        const Vc2MatrixWords before = r.m;
        const std::string what = vc2BothOn(r.fw, kRefresh, { kMatrixAt }, "the refresh", [&] { r.core.refresh(r.m); },
                                           [&] { return matrixDifference(r); });
        r.n.moved += std::memcmp(before.dirty, r.m.dirty, sizeof before.dirty) != 0;
        return what;
    }
    if (pick < 8) {
        ++r.n.reads;
        const size_t g = rng() % kReaders.size();
        if (vc2MatrixGroups().size() != kReaders.size()) return "the port's groups";
        const int16_t* sums = vc2MatrixRead(r.m, vc2MatrixGroups()[g], r.core);
        const auto at = (uint32_t)((const uint8_t*)sums - (const uint8_t*)&r.m);
        return answerOn(r, kReaders[g], { kMatrixAt }, kMatrixAt + at, "group " + std::to_string(g) + "'s sums' place");
    }
    if (pick < 10) {
        ++r.n.sums;
        const int dest = (int)(rng() % Vc2MatrixTables::kDestinations);
        return answerOn(r, kGetter, { kMatrixAt, (uint32_t)dest }, (uint32_t)vc2MatrixSum(r.m, dest),
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
    state.channel = &channel;
}

void MatrixRig::start(std::mt19937& rng) {
    vc2CosmNoise(m, rng);
    drawMatrixCore(m, rng);
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
