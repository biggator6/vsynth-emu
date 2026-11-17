// xt_vc2cosm's fourth part (v247): rom.4's Matrix set-up against Vc2MatrixSetup, on G at its own address (0x89997398)
// and M in the interpreter's RAM: a part's record (0x893a46f0), the system's assignments (0x893a50b0), both
// (0x893a51c0), M's init from its part's record (0x893a2ee0) with the levels, the levels alone (0x893a2e50), a note's
// head (0x893a2eb0), and the two destination routines called alone (0x893a30d0, 0x893a36a0).  Only what lies outside M
// is hooked: the readers' set-up (0x893207b0, 0x89320790) and the getter (0x89324350), answering both sides from one
// drawn store, and 0x893b1ee0; the refresh and the sources run as rom.4's.  Each case G, M and the sources' world
// drawn, then 60 events; after each G, M with its links, the levels, both logs of calls and the answer, word for word.
// Its draws and store in Vc2MatrixSetupDraws.cpp.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2MatrixSetupDraws.h"

namespace {
constexpr uint32_t kRecord = 0x893a46f0, kAssigns = 0x893a50b0, kSettings = 0x893a51c0, kInit = 0x893a2ee0;   // [FW]
constexpr uint32_t kLevels = 0x893a2e50, kHead = 0x893a2eb0, kDest = 0x893a30d0, kDepth = 0x893a36a0;         // [FW]
constexpr uint32_t kRefresh = 0x893a2230;                                                                   // [FW]
// [FW] outside M: the readers' set-up, a part's and the system's, the getter, and what the levels call
constexpr uint32_t kPartReader = 0x893207b0, kSystemReader = 0x89320790, kGetter = 0x89324350;
constexpr uint32_t kLevelsCall = 0x893b1ee0;
constexpr uint32_t kSettingsAt = 0x89997398, kLevelsAt = 0x89997290, kHeldAt = 0x8993d0c0;   // [FW]
// In Vc2Fw's RAM: the uses words and a head's bytes for the routines called alone
constexpr uint32_t kUsesAt = 0x8C073000, kBytesAt = 0x8C073100;
constexpr uint32_t kPartHandle = 0x1000, kSystemHandle = 0x2000;   // what the readers' set-up leaves for the getter
constexpr int kEvents = 60;

struct SetupCounts {
    long events = 0, records = 0, held = 0, assigns = 0, settings = 0, inits = 0, levels = 0, heads = 0, dests = 0,
         depths = 0, draws = 0, reads = 0;
};

// The port's parameters and 0x893b1ee0: the store's values, each call logged as rom.4's hooks log theirs
struct PortParams final : Vc2MatrixParams {
    PortParams(SetupStore& s, std::vector<SetupCall>& l) : store(s), log(l) {}
    SetupStore& store;
    std::vector<SetupCall>& log;
    int32_t partValue(int part, int16_t id) override { return logged(part, id); }
    int32_t systemValue(int16_t id) override { return logged(kSetupSystem, id); }
    int32_t logged(int block, int16_t id) {
        log.push_back({ block, id });
        return store.value(block, id);
    }
};
struct PortPart final : Vc2MatrixPart {
    explicit PortPart(std::vector<SetupCall>& l) : log(l) {}
    std::vector<SetupCall>& log;
    void levelsMoved() override { log.push_back({ kSetupLevels, 0 }); }
};

struct SetupRig {
    explicit SetupRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    SetupStore store;
    std::vector<SetupCall> romCalls, portCalls;
    Vc2MatrixSettings g {};
    Vc2MatrixWords m {};
    Vc2MatrixLinks links {};
    Vc2MatrixWorld world { Vc2MatrixSettings::kParts };
    bool held = false;
    Vc2MatrixSourceState state { &m };
    Vc2MatrixSourceRoutines sources { state };
    Vc2MatrixCore core { sources };
    PortParams params { store, portCalls };
    PortPart part { portCalls };
    Vc2MatrixSetup setup { params, part };
    SetupCounts n;
    // vc2CosmPart's: rom.4's side hooked, a case's start, an event, the counts said
    void hook();
    void start(std::mt19937& rng);
    std::string event(std::mt19937& rng);
    void say(const Vc2Run& run) const;
};

// G, M with its links, the world, the levels and the held flag into rom.4's memory; the port's sources on the same
void writeSetup(SetupRig& r) {
    Sh4& cpu = r.fw.cpu;
    cpu.write(kSettingsAt, &r.g, sizeof r.g);
    cpu.write(kVc2MatrixAt, &r.m, sizeof r.m);
    cpu.write(kVc2MatrixAt + kVc2MatrixLinks, &r.links, sizeof r.links);
    cpu.write(kLevelsAt, r.setup.levelsOut.values.data(), sizeof r.setup.levelsOut.values);
    cpu.wr8(kHeldAt, r.held ? 1 : 0);
    r.world.write(cpu);
    cpu.write(kSetupChannelCopyAt, &r.world.channel, sizeof r.world.channel);
    std::ranges::copy(r.g.assigns, r.state.assigns.begin());
    r.state.partByte = r.world.partBytes[(size_t)r.links.part];
    r.romCalls.clear();
    r.portCalls.clear();
}

std::string setupDifference(SetupRig& r) {
    const std::string calls = vc2CallsDifference(r.romCalls, r.portCalls, [](const SetupCall& c) {
        return "block " + std::to_string(c.kind) + " ID " + std::to_string(c.id);
    });
    if (!calls.empty()) return calls;
    return vc2RegionsDifference(r.fw, {
        { kSettingsAt, &r.g, sizeof r.g, "the Matrix's settings" },
        { kVc2MatrixAt, &r.m, sizeof r.m, "the Matrix object" },
        { kVc2MatrixAt + kVc2MatrixLinks, &r.links, sizeof r.links, "M's channel word and part" },
        { kLevelsAt, r.setup.levelsOut.values.data(), sizeof r.setup.levelsOut.values, "the levels" } });
}

// rom.4's `routine` on `args`, then the port's `mine`, whose answer is r0's when `answered`
template <class Mine>
std::string setupBoth(SetupRig& r, uint32_t routine, std::initializer_list<uint32_t> args, const char* what, Mine mine,
                      bool answered = true) {
    uint32_t r0 = 0;
    if (!r.fw.call(routine, args, &r0)) return std::string(what) + " faults: " + r.fw.cpu.error();
    const uint32_t answer = mine();
    const std::string differs = setupDifference(r);
    if (!differs.empty()) return std::string(what) + ": " + differs;
    return answered && r0 != answer ? std::string(what) + "'s answer" : "";
}

// A part's record on both, rom.4's `routine` on G and a drawn part, the port's `mine(part)`: 0x893a46f0 alone, or
// 0x893a51c0 with the assignments first
template <class Mine>
std::string recordOn(SetupRig& r, std::mt19937& rng, long& count, uint32_t routine, const char* what, Mine mine) {
    ++count;
    r.n.held += r.held;
    const auto p = (int)(rng() % Vc2MatrixSettings::kParts);
    writeSetup(r);
    return setupBoth(r, routine, { kSettingsAt, (uint32_t)p }, what, [&] { return (uint32_t)mine(p); });
}

// The events, by what they run
using SetupEvent = std::string (*)(SetupRig&, std::mt19937&);

std::string onStore(SetupRig& r, std::mt19937& rng) {
    ++r.n.draws;
    r.store.redraw(rng);
    return "";
}

std::string onState(SetupRig& r, std::mt19937& rng) {
    ++r.n.draws;
    r.held = rng() % 4 == 0;
    r.links.part = (int16_t)(rng() % Vc2MatrixSettings::kParts);
    r.links.channel = kSetupChannels[rng() % kSetupChannels.size()];
    r.world.drawChannel(rng, vc2MatrixLimit((int)(rng() % Vc2MatrixTables::kDestinations)));
    r.world.drawParts(rng);
    vc2DrawMatrixCore(r.m, rng);
    return "";
}

std::string onSettingsDraw(SetupRig& r, std::mt19937& rng) {
    ++r.n.draws;
    vc2DrawSettings(r.g, rng);
    return "";
}

std::string onRecord(SetupRig& r, std::mt19937& rng) {
    return recordOn(r, rng, r.n.records, kRecord, "a record", [&](int p) { return r.setup.record(r.g, p, r.held); });
}

std::string onPartSettings(SetupRig& r, std::mt19937& rng) {
    return recordOn(r, rng, r.n.settings, kSettings, "the settings",
                    [&](int p) { return r.setup.settings(r.g, p, r.held); });
}

std::string onAssigns(SetupRig& r, std::mt19937&) {
    ++r.n.assigns;
    writeSetup(r);
    return setupBoth(r, kAssigns, { kSettingsAt }, "the assignments", [&] {
        r.setup.assigns(r.g);
        return 1u;
    });
}

std::string onInit(SetupRig& r, std::mt19937&) {
    ++r.n.inits;
    writeSetup(r);
    return setupBoth(r, kInit, { kVc2MatrixAt }, "M's init", [&] {
        r.setup.init(r.m, r.links, r.g, r.core);
        return 1u;
    });
}

std::string onLevels(SetupRig& r, std::mt19937&) {
    ++r.n.levels;
    writeSetup(r);
    return setupBoth(r, kLevels, { kVc2MatrixAt }, "the levels", [&] {
        r.setup.levels(r.m, r.core);
        return 0u;
    }, false);
}

std::string onHead(SetupRig& r, std::mt19937& rng) {
    ++r.n.heads;
    const int32_t p = rng() % 3 ? r.links.part : (int32_t)(rng() % Vc2MatrixSettings::kParts);
    const std::array<uint8_t, 3> bytes { vc2DrawMidiByte(rng), vc2DrawMidiByte(rng), vc2DrawMidiByte(rng) };
    writeSetup(r);
    r.fw.cpu.write(kBytesAt, bytes.data(), bytes.size());
    return setupBoth(r, kHead, { kVc2MatrixAt, (uint32_t)p, kBytesAt }, "the head", [&] {
        vc2MatrixHead(r.m, r.links, p, bytes);
        return 0u;
    }, false);
}

std::string onDest(SetupRig& r, std::mt19937& rng) {
    ++r.n.dests;
    const SetupArgs a = vc2DrawSetupArgs(rng);
    std::array<uint16_t, Vc2MatrixFront::kGroups + 1> uses {};   // and a word past them: the compare's whole words
    for (uint16_t& u : uses) u = rng() % 2 ? 0 : (uint16_t)rng();
    writeSetup(r);
    r.fw.cpu.write(kUsesAt, uses.data(), sizeof uses);
    uint32_t r0 = 0;
    if (!r.fw.call(kDest, { (uint32_t)a.source, (uint32_t)a.dest, (uint32_t)a.sens, kUsesAt }, &r0))
        return "the destination faults: " + r.fw.cpu.error();
    const auto mine = (uint32_t)vc2MatrixDestination(a.source, a.dest, a.sens, uses.data());
    const std::string differs = vc2RegionsDifference(r.fw, { { kUsesAt, uses.data(), sizeof uses, "the uses" } });
    return !differs.empty() ? differs : r0 != mine ? "the destination of " + std::to_string(a.dest) : "";
}

std::string onDepth(SetupRig& r, std::mt19937& rng) {
    ++r.n.depths;
    const SetupArgs a = vc2DrawSetupArgs(rng);
    writeSetup(r);
    uint32_t r0 = 0;
    if (!r.fw.call(kDepth, { (uint32_t)a.source, (uint32_t)a.dest, (uint32_t)a.sens }, &r0))
        return "the depth faults: " + r.fw.cpu.error();
    const auto mine = (uint32_t)(int32_t)vc2MatrixDepth((uint8_t)a.source, a.dest, a.sens);
    return r0 != mine ? "the depth of " + std::to_string(a.dest) + " at " + std::to_string(a.sens) : "";
}

// Each event's share of sixteen
constexpr std::array<SetupEvent, 16> kSetupEvents { onStore, onState, onSettingsDraw, onRecord, onRecord, onRecord,
    onAssigns, onPartSettings, onPartSettings, onInit, onInit, onLevels, onHead, onDest, onDepth, onDepth };

// rom.4's side: the readers' set-up leaves a handle for the getter, which answers from the store; both log
void SetupRig::hook() {
    Sh4& cpu = fw.cpu;
    cpu.unhook(kRefresh);
    state.channel = &world.channel;
    cpu.hook(kPartReader, [](Sh4& c) {
        c.wr32(c.r[4] + 4, kPartHandle + c.r[5]);
        c.ret(0);
    });
    cpu.hook(kSystemReader, [](Sh4& c) {
        c.wr32(c.r[4] + 4, kSystemHandle);
        c.ret(0);
    });
    cpu.hook(kGetter, [this](Sh4& c) {
        const int block = c.r[4] == kSystemHandle ? kSetupSystem : (int)(c.r[4] - kPartHandle);
        const auto id = (int16_t)c.r[5];
        ++n.reads;
        romCalls.push_back({ block, id });
        c.ret((uint32_t)store.value(block, id));
    });
    cpu.hook(kLevelsCall, [this](Sh4& c) {
        romCalls.push_back({ kSetupLevels, 0 });
        c.ret(0);
    });
}

void SetupRig::start(std::mt19937& rng) {
    store.redraw(rng);
    vc2DrawSettings(g, rng);
    vc2CosmNoise(m, rng);
    held = false;
    onState(*this, rng);
}

std::string SetupRig::event(std::mt19937& rng) {
    ++n.events;
    return kSetupEvents[rng() % kSetupEvents.size()](*this, rng);
}

void SetupRig::say(const Vc2Run& run) const {
    std::printf("setup: %d cases, %ld events: %ld records (%ld held), %ld assignments, %ld settings, %ld inits, %ld "
                "levels, %ld heads, %ld destinations, %ld depths, %ld draws; %ld parameter reads\n", run.cases,
                n.events, n.records, n.held, n.assigns, n.settings, n.inits, n.levels, n.heads, n.dests, n.depths,
                n.draws, n.reads);
}
} // namespace

long vc2MatrixSetupCases(Vc2Fw& fw, const Vc2Run& run) { return vc2CosmPart<SetupRig>(fw, run, 3, kEvents); }
#endif
