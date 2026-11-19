// xt_vc2settings' sixteenth part (v302): the controller numbers and the part's init, Vc2PartNumbers, against rom.4's
// own init 0x89393370 and its own number objects through the controller 0x89392f90, on the seed fifteen after the
// run's.  The number classes' twelve handlers (each vtable's +0x14) hooked to log their source, channel, value and
// address, then their object; the earlier parts' hooks on the controller and the 120 class's maker taken off; nothing
// else hooked.  Each case
// every table, the nodes, the lists, the part objects (an index 0…15) and the keys noise; events: the init (one in
// four, and in place of a controller while a list is not empty: the controller walks the lists), or the controller (a
// source 1…4, a channel 0…15, a number 0…127, a value), one in four after an object's class drawn as another object's
// and its number byte noise, now and then a table word pointing at another object.  The number objects stay rom.4's
// classes from the part's start (the port's made then).  After each the tables, the nodes, the number objects, their
// table, the lists, the part objects, the keys' span and the calls.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartControllersCheck.h"
#include "Vc2PartSettingsStates.h"
#include <algorithm>

namespace {
using P = Vc2PartNumbers;
constexpr uint32_t kInit = 0x89393370;   // [FW]
// [FW] the number classes' handlers, the check's own list
constexpr uint32_t kHandlers[] = { 0x893938d0, 0x89393920, 0x89393950, 0x89393980, 0x89393a10, 0x89393a70,
                                   0x89393ac0, 0x89393ba0, 0x89393c10, 0x89393c80, 0x89393ce0, 0x89393d90 };
constexpr uint32_t kObjectsBytes = sizeof(Vc2NumberObject) * P::kNumbers, kTableBytes = 4 * P::kNumbers;

SettingsCall handled(Sh4& c) {
    return { kOutHandler, (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[7], (int32_t)c.pc };
}
SettingsCall handledObject(Sh4& c) { return { kOutHandlerObject, 0, 0, 0, (int32_t)c.r[4] }; }
constexpr auto kHandlerCallees = [] {
    std::array<SettingsCallee, std::size(kHandlers)> a {};
    for (size_t k = 0; k < a.size(); ++k) a[k] = { kHandlers[k], handled, nullptr, handledObject };
    return a;
}();
// [FW] the controller, and the 120 class's maker (the sixth part's hook, which logs v291's All Sound Off)
constexpr uint32_t kNumbersOwn[] = { kController, 0x89393c00 };

// The number objects rom.4's classes from the start: the port's made, written into rom.4's memory before each event;
// and the sources' rows of channel states as the image holds them, which the keys' static initializer reads (the
// seventh and eleventh parts draw the first source's in rom.4's memory)
void hookNumbers(SettingsSides& s) {
    s.numbers.make(s.rows);
    const auto& sources = vc2KeysTables().sources;
    s.fw.cpu.write(kStateSourceAt, sources.data(), sources.size());
    settingsHookCallees(s, kHandlerCallees, kNumbersOwn);
}

// The port's handlers' calls, logged as rom.4's hooks log theirs
class NumberPortParts final : public Vc2NumberParts {
public:
    explicit NumberPortParts(std::vector<SettingsCall>& log) : log_(log) {}
    void handle(uint32_t handler, uint32_t object, int source, int32_t channel, int32_t value) override {
        log_.push_back({ kOutHandler, source, channel, value, (int32_t)handler });
        log_.push_back({ kOutHandlerObject, 0, 0, 0, (int32_t)object });
    }

private:
    std::vector<SettingsCall>& log_;
};

// A routine on both sides, the nodes, the number objects, their table, the lists and the part objects into rom.4's
// memory first; then those and the keys' span compared
std::string numbersOn(SettingsSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                      const std::function<void()>& mine) {
    Sh4& cpu = s.fw.cpu;
    P& p = s.numbers;
    Vc2PartNodes& n = s.nodes;
    cpu.write(Vc2PartNodes::kNodesAt, n.nodes.data(), sizeof n.nodes);
    cpu.write(P::kObjectsAt, p.objects.data(), kObjectsBytes);
    cpu.write(P::kTableAt, p.table.data(), kTableBytes);
    cpu.write(Vc2PartNodes::kListsAt, n.lists.data(), sizeof n.lists);
    cpu.write(Vc2PartNodes::kObjectsAt, n.objects.data(), sizeof n.objects);
    const std::string what = settingsOnBoth(s, r, args, [&] {
        mine();
        return 0u;
    });
    if (!what.empty()) return what;
    return vc2RegionsDifference(s.fw, { { P::kObjectsAt, p.objects.data(), kObjectsBytes, "the number objects" },
                                        { P::kTableAt, p.table.data(), kTableBytes, "their table" },
                                        { Vc2PartNodes::kListsAt, n.lists.data(), sizeof n.lists, "the lists" },
                                        { Vc2PartNodes::kObjectsAt, n.objects.data(), sizeof n.objects, "objects" },
                                        { Vc2Keys::kTablesAt, &s.keys, sizeof s.keys, "the keys" } });
}

// The init, part object 0's index drawn 0…15 (its glides' reset reads it)
std::string initEvent(SettingsSides& s, std::mt19937& rng) {
    s.nodes.objects[0].index = (uint8_t)(rng() % Vc2PartNodes::kObjects);
    ++s.n.byEvent[67];
    return numbersOn(s, { kInit, "the part's init", nullptr }, {}, [&] {
        Vc2PartInitWorld w { s.rows, s.nodes, s.keys, s.numbers };
        vc2PartInit(w);
    });
}

// An object's class drawn as another object's, its number byte noise, now and then its table word another object's
void redrawObject(SettingsSides& s, std::mt19937& rng) {
    P& p = s.numbers;
    const size_t k = rng() % P::kNumbers;
    p.objects[k].vtable = p.objects[rng() % P::kNumbers].vtable;
    p.objects[k].number = (uint8_t)rng();
    if (rng() % 4 == 0) p.table[k] = P::kObjectsAt + (uint32_t)(sizeof(Vc2NumberObject) * (rng() % P::kNumbers));
    ++s.n.byEvent[69];
}

// The controller on the number objects, the lists empty (else the init)
std::string numberEvent(SettingsSides& s, std::mt19937& rng) {
    const auto empty = (size_t)std::ranges::count(s.nodes.lists, 0u);
    if (empty != s.nodes.lists.size() || rng() % 4 == 0) return initEvent(s, rng);
    if (rng() % 4 == 0) redrawObject(s, rng);
    const int source = 1 + (int)(rng() % Vc2PartNodes::kSources);
    const auto channel = (int32_t)(rng() % Vc2PartNodes::kChannels);
    const auto number = (int32_t)(rng() % P::kNumbers);
    const auto value = (int32_t)(rng() % 2 ? rng() % 128 : rng());
    ++s.n.byEvent[68];
    const std::initializer_list<uint32_t> args { (uint32_t)source, (uint32_t)channel, (uint32_t)number,
                                                 (uint32_t)value };
    return numbersOn(s, { kController, "the controller on the numbers", nullptr }, args, [&] {
        ControllerPortParts parts { s };
        NumberPortParts numbers { s.mine.log };
        parts.number = [&](int32_t n, int src, int32_t ch, int32_t v) {
            s.numbers.changed(n, src, ch, v, numbers);
        };
        Vc2PartControllers { s.rows, s.nodes, parts }.controller(source, channel, number, value);
    });
}

const SettingsEvent kNumberEvents[] = { numberEvent };

// A case: the nodes, the lists, the part objects (an index 0…15) and the keys noise; then the tables' and the events
std::string numbersCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    Vc2PartNodes& n = s.nodes;
    settingsNoise(n.nodes.data(), sizeof n.nodes, rng);
    settingsNoise(n.lists.data(), sizeof n.lists, rng);
    settingsNoise(n.objects.data(), sizeof n.objects, rng);
    for (Vc2PartObject& o : n.objects) o.index = (uint8_t)(rng() % Vc2PartNodes::kObjects);
    settingsNoise(&s.keys, sizeof s.keys, rng);
    s.fw.cpu.write(Vc2Keys::kTablesAt, &s.keys, sizeof s.keys);
    return settingsCaseOf<kNumberEvents, sizeof(Vc2PartRows), 5>(s, rng, events, calls);
}

constexpr SettingsSaid kNumberSaid[] = { { 67, "inits" }, { 68, "controllers" }, { 69, "objects drawn" } };
} // namespace

const SettingsPartRow kNumberPartRow { 15, &SettingsCounts::numberEvents, &SettingsCounts::numberCalls, numbersCase,
                                       nullptr, hookNumbers, "controller numbers", kNumberSaid };
#endif
