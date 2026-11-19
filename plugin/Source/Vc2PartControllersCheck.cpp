// xt_vc2settings' fifteenth part (v301): the controllers, Vc2PartControllers, against rom.4's own 0x89392f90 and
// 0x89394440, on the seed fourteen after the run's.  rom.4's number objects (0x899891e0, 128) are made here, each at an
// address of its own with one vtable whose +0x14 is a hooked address that logs the number; the part table (0x8993d040)
// points at 16 trip objects' fronts in scratch memory; each node's word points at a byte of its own, the part drawn
// 0…15.  Each case every table, the nodes noise with their lists drawn as xt_vc2keys' second part draws them, and the
// trip objects noise; before each event Time Trip Assign X and Y drawn (0…127, 121 or −1, now and then the same), a
// part's trip object's flag (0, 1 or any), X and Y drawn now and then, and the trip's move's answer.  Hooked to log: the
// number objects' virtual and the trip object's 0x893b0610 and 0x893b0460 (answering as drawn).  Events: the controller
// (a source 1…4, a channel 0…15 or, from source 2, −1, which reads the source before's channel 15; a number Time Trip
// Assign X or Y or 121 half the time, else 0…127) and the part's controller alone (any number).  After each the tables,
// the trip objects and the calls.  Not drawn: a controller's number past 0…127 (rom.4 reads past its table).
// XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartControllersCheck.h"
#include "Vc2PartNodesDraws.h"

namespace {
constexpr uint32_t kNumbersAt = 0x899891e0;                                           // [FW]
constexpr uint32_t kVirtualAt = 0x14, kAdjustAt = 0x0c, kVtableWordAt = 4;            // [FW] the number object's
// The check's own: the number objects, their vtable, its virtual, the nodes' bytes
constexpr uint32_t kObjectsAt = 0x8C040000, kObjectBytes = 16, kVtableAt = 0x8C041000, kHookAt = 0x8C041100;
constexpr uint32_t kBytesAt = 0x8C043000;
constexpr int kNumbers = 128;
constexpr int32_t kResetAll = 121;

SettingsCall numberCalled(Sh4& c) {
    const auto number = (int32_t)((c.r[4] - kObjectsAt) / kObjectBytes);
    return { kOutNumber, (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[7], number };
}
SettingsCall setCalled(Sh4& c) {
    return { kOutTripSet, (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[7], (int32_t)c.r[4] };
}
SettingsCall moveCalled(Sh4& c) { return { kOutTripMove, 0, (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[4] }; }
uint32_t moveAnswered(SettingsSides& s) { return s.tripAnswer; }
constexpr SettingsCallee kControllerCallees[] = { { kHookAt, numberCalled }, { kTripSetAt, setCalled },
                                                  { kTripMoveAt, moveCalled, moveAnswered } };
constexpr uint32_t kControllerOwn[] = { kController };   // the Category's, receive's and routing's hook taken off

// The number objects, their vtable (no adjustment, the virtual at the hooked address) and the part table, made once
void hookControllers(SettingsSides& s) {
    Sh4& cpu = s.fw.cpu;
    for (uint32_t n = 0; n < kNumbers; ++n) {
        cpu.wr32(kNumbersAt + 4 * n, kObjectsAt + kObjectBytes * n);
        cpu.wr32(kObjectsAt + kObjectBytes * n + kVtableWordAt, kVtableAt);
    }
    cpu.wr32(kVtableAt + kAdjustAt, 0);
    cpu.wr32(kVtableAt + kVirtualAt, kHookAt);
    for (uint32_t p = 0; p < kSettingsParts; ++p) cpu.wr32(kPartTableAt + 4 * p, kTripsAt + sizeof(Vc2TripFront) * p);
    settingsHookCallees(s, kControllerCallees, kControllerOwn);
}

// Time Trip Assign X and Y (0…127, 121 or −1, now and then the same), a part's trip object now and then, the answer
void drawControllerWords(SettingsSides& s, std::mt19937& rng) {
    auto& a = s.rows.tripAssigns;
    for (int8_t& b : a) {
        const std::array<int8_t, 4> picks { -1, (int8_t)kResetAll, (int8_t)(rng() % kNumbers), (int8_t)(rng() % 8) };
        b = picks[rng() % picks.size()];
    }
    if (rng() % 8 == 0) a[1] = a[0];
    s.fw.cpu.write(kTripAssignsAt, a.data(), a.size());
    if (rng() % 2 == 0) {
        Vc2TripFront& t = s.trips[rng() % kSettingsParts];
        const std::array<uint32_t, 3> flags { 0, 1, (uint32_t)rng() };
        t.moving = flags[rng() % flags.size()];
        t.x = (int8_t)rng();
        t.y = (int8_t)rng();
    }
    s.tripAnswer = (uint8_t)rng();
}

// A value: 0, 1, 64, 127, −128 as a byte, −1, or any word
int32_t drawValue(std::mt19937& rng) {
    const std::array<int32_t, 7> values { 0, 1, 64, 127, 0x80, -1, (int32_t)rng() };
    return values[rng() % values.size()];
}

// A number: Time Trip Assign X or Y or 121 half the time (within the table for the controller), else 0…127
int32_t drawNumber(SettingsSides& s, std::mt19937& rng, bool inTable) {
    const std::array<int32_t, 3> named { s.rows.tripAssigns[0], s.rows.tripAssigns[1], kResetAll };
    const int32_t n = named[rng() % named.size()];
    const bool takeNamed = rng() % 2 == 0 && (!inTable || (n >= 0 && n < kNumbers));
    return takeNamed ? n : (int32_t)(rng() % kNumbers);
}

// A routine on both sides, the lists and the trip objects into rom.4's memory first; then the trip objects
std::string controllerOn(SettingsSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                         const std::function<void(Vc2PartControllers&)>& mine) {
    ControllerPortParts parts { s };
    Vc2PartControllers controllers { s.rows, s.nodes, parts };
    s.fw.cpu.write(Vc2PartNodes::kListsAt, s.nodes.lists.data(), sizeof s.nodes.lists);
    s.fw.cpu.write(kTripsAt, s.trips.data(), sizeof s.trips);
    const std::string what = settingsOnBoth(s, r, args, [&] {
        mine(controllers);
        return 0u;
    });
    for (const SettingsCall& c : s.mine.log) {
        s.n.byEvent[65] += c.block == kOutTripSet;
        s.n.byEvent[66] += c.block == kOutTripMove;
    }
    if (!what.empty()) return what;
    return vc2RegionsDifference(s.fw, { { kTripsAt, s.trips.data(), sizeof s.trips, "the trip objects" } });
}

std::string controllerEvent(SettingsSides& s, std::mt19937& rng) {
    drawControllerWords(s, rng);
    const int source = 1 + (int)(rng() % Vc2PartNodes::kSources);
    const int32_t channel = source > 1 && rng() % 8 == 0 ? -1 : (int32_t)(rng() % Vc2PartNodes::kChannels);
    const int32_t number = drawNumber(s, rng, true);
    const int32_t value = drawValue(rng);
    ++s.n.byEvent[63];
    const std::initializer_list<uint32_t> args { (uint32_t)source, (uint32_t)channel, (uint32_t)number,
                                                 (uint32_t)value };
    return controllerOn(s, { kController, "the controller", nullptr }, args,
                        [&](Vc2PartControllers& c) { c.controller(source, channel, number, value); });
}

std::string partControllerEvent(SettingsSides& s, std::mt19937& rng) {
    drawControllerWords(s, rng);
    const auto part = (uint8_t)(rng() % kSettingsParts);
    const int32_t number = rng() % 16 == 0 ? (int32_t)rng() : drawNumber(s, rng, false);
    const int32_t value = drawValue(rng);
    ++s.n.byEvent[64];
    return controllerOn(s, { kPartController, "the part's controller", nullptr },
                        { part, (uint32_t)number, (uint32_t)value },
                        [&](Vc2PartControllers& c) { c.partController(part, number, value); });
}

const SettingsEvent kControllerEvents[] = { controllerEvent, partControllerEvent };

// A case: the nodes noise, their lists drawn, each one's word a byte of its own with a part 0…15; the trip objects
// noise; then the tables' and the events
std::string controllerCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    Vc2PartNodes& n = s.nodes;
    settingsNoise(n.nodes.data(), sizeof n.nodes, rng);
    vc2DrawNodeLists(n, rng);
    for (uint32_t k = 0; k < (uint32_t)kVc2Nodes; ++k) {
        n.nodes[k].word8 = kBytesAt + k;
        s.fw.cpu.wr8(kBytesAt + k, (uint8_t)(rng() % kSettingsParts));
    }
    settingsNoise(s.trips.data(), sizeof s.trips, rng);
    return settingsCaseOf<kControllerEvents, sizeof(Vc2PartRows), 5>(s, rng, events, calls);
}

constexpr SettingsSaid kControllerSaid[] = { { 63, "controllers" }, { 64, "part controllers" }, { 65, "trip sets" },
                                             { 66, "trip moves" } };
} // namespace

const SettingsPartRow kControllerPartRow { 14, &SettingsCounts::controllerEvents, &SettingsCounts::controllerCalls,
                                           controllerCase, nullptr, hookControllers, "controllers", kControllerSaid };
#endif
