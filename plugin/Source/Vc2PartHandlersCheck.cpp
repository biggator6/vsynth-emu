// xt_vc2settings' seventeenth part (v303): the number classes' handlers, Vc2PartNumbers' own, against rom.4's own
// through its controller 0x89392f90 on its own number objects, on the seed sixteen after the run's.  The sixteenth
// part's hooks on the twelve handlers taken off; hooked to log: the listeners of a channel's state 0x89390ea0, the
// vocoder's pedal 0x893b1c10 and v278's five key-table routines (0x89390cd0, 0x89390d10, 0x89390d50, 0x89390da0,
// 0x89390e20); the part's controller's trip routines as the fifteenth part's.  Each case every table noise, the nodes
// noise with their lists drawn as xt_vc2keys' second part draws them (each one's object index 0…15, its word a byte of
// its own with a part 0…15), the keys and the trip objects noise.  Events: the controller (a source 1…4, a channel
// 0…15, a number a class's own half the time, else 0…127, a value at the handlers' edges or any), before each the held
// note's word 0 half the time and the channel's Hold-press byte 0, 1 or any; one time in 16 from source 2 no channel
// (−1) on a number of the base's.  After each the tables, the keys' span, the trip objects and the calls.  Not drawn: no
// channel on the other classes (Hold, Sostenuto, CC 84, Reset All Controllers and the notes off read before their
// tables or bytes with it).  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartControllersCheck.h"

namespace {
template <int32_t Which>
SettingsCall keyRoutineCalled(Sh4& c) {
    return { kOutKeyRoutine, Which, 0, 0, (int32_t)c.r[4] };
}
SettingsCall notifyCalled(Sh4& c) { return { kOutNotify, 0, 0, 0, (int32_t)c.r[4] }; }
SettingsCall pedalCalled(Sh4& c) { return { kOutVocoderHold, 0, 0, 0, (int32_t)c.r[4] }; }
constexpr SettingsCallee kHandlerCallees[] = {
    { 0x89390ea0, notifyCalled },        { 0x893b1c10, pedalCalled },         { 0x89390cd0, keyRoutineCalled<0> },
    { 0x89390d10, keyRoutineCalled<1> }, { 0x89390d50, keyRoutineCalled<2> }, { 0x89390da0, keyRoutineCalled<3> },
    { 0x89390e20, keyRoutineCalled<4> } };
// The handlers rom.4's own again, and the controller
constexpr auto kHandlersOwn = [] {
    std::array<uint32_t, std::size(kNumberHandlers) + 1> a {};
    for (size_t k = 0; k < std::size(kNumberHandlers); ++k) a[k] = kNumberHandlers[k];
    a.back() = kController;
    return a;
}();

void hookHandlers(SettingsSides& s) { settingsHookCallees(s, kHandlerCallees, kHandlersOwn); }

// The port's calls out, logged as rom.4's hooks log theirs
class HandlerPortParts final : public Vc2NumberParts {
public:
    explicit HandlerPortParts(std::vector<SettingsCall>& log) : log_(log) {}
    void notify(uint32_t state) override { logOut(log_, kOutNotify, state); }
    void vocoderHold(int32_t value) override { logOut(log_, kOutVocoderHold, (uint32_t)value); }
    void holdOff(uint32_t table) override { log_.push_back({ kOutKeyRoutine, 0, 0, 0, (int32_t)table }); }
    void sostenutoOff(uint32_t table) override { log_.push_back({ kOutKeyRoutine, 1, 0, 0, (int32_t)table }); }
    void truncateTable(uint32_t table) override { log_.push_back({ kOutKeyRoutine, 2, 0, 0, (int32_t)table }); }
    void releaseBut(uint32_t table) override { log_.push_back({ kOutKeyRoutine, 3, 0, 0, (int32_t)table }); }
    void releaseAll(uint32_t table) override { log_.push_back({ kOutKeyRoutine, 4, 0, 0, (int32_t)table }); }

private:
    std::vector<SettingsCall>& log_;
};

// [FW] each class's numbers, one of which an event takes half the time; the base's and those that hand on to it, which
// alone take no channel (−1) as nothing
constexpr int32_t kClassNumbers[] = { 1, 7, 5, 64, 66, 84, 121, 120, 126, 127, 123, 124, 125, 38, 6 };
constexpr int32_t kBaseNumbers[] = { 0, 1, 2, 7, 10, 5, 65, 6, 38 };
// The values at the handlers' edges: 0, 16 and 17 (Mono), 63 and 64 (Hold), −1 (the notes off), and others
constexpr int32_t kHandlerValues[] = { 0, 1, 16, 17, 63, 64, 127, -1, -128 };

std::string handlerEvent(SettingsSides& s, std::mt19937& rng) {
    const int source = 1 + (int)(rng() % Vc2PartNodes::kSources);
    // no channel one time in 16 from source 2 (the controller's list walk reads before the lists with it from source
    // 1), on a number of the base's
    const bool none = source > 1 && rng() % 16 == 0;
    const int32_t channel = none ? -1 : (int32_t)(rng() % Vc2PartNodes::kChannels);
    const int32_t named = rng() % 2 ? kClassNumbers[rng() % std::size(kClassNumbers)] : (int32_t)(rng() % 128);
    const int32_t number = none ? kBaseNumbers[rng() % std::size(kBaseNumbers)] : named;
    const int32_t value = rng() % 4 ? kHandlerValues[rng() % std::size(kHandlerValues)] : (int32_t)rng();
    Vc2PartRows& t = s.rows;
    const std::array<uint32_t, 4> held { 0, 0, 1, (uint32_t)rng() };
    t.heldNote = held[rng() % held.size()];
    const std::array<uint8_t, 3> presses { 0, 1, (uint8_t)rng() };
    t.holdPresses[(size_t)(none ? 0 : channel)] = presses[rng() % presses.size()];
    Sh4& cpu = s.fw.cpu;
    cpu.wr32(kHeldNoteAt, t.heldNote);
    cpu.write(kHoldPressesAt, t.holdPresses.data(), t.holdPresses.size());
    cpu.write(Vc2PartNodes::kListsAt, s.nodes.lists.data(), sizeof s.nodes.lists);
    cpu.write(kTripsAt, s.trips.data(), sizeof s.trips);
    ++s.n.byEvent[70];
    const std::initializer_list<uint32_t> args { (uint32_t)source, (uint32_t)channel, (uint32_t)number,
                                                 (uint32_t)value };
    const std::string what = settingsOnBoth(s, { kController, "the controller's handlers", nullptr }, args, [&] {
        ControllerPortParts parts { s };
        HandlerPortParts handlers { s.mine.log };
        Vc2NumberWorld w { s.rows, s.nodes, s.keys, handlers };
        parts.number = [&](int32_t n, int src, int32_t ch, int32_t v) {
            s.numbers.changed(n, src, ch, v, w);
        };
        Vc2PartControllers { s.rows, s.nodes, parts }.controller(source, channel, number, value);
        return 0u;
    });
    settingsCountCalls(s, { { kOutKeyRoutine, 71 }, { kOutNotify, 72 } });
    if (!what.empty()) return what;
    return vc2RegionsDifference(s.fw, { { Vc2Keys::kTablesAt, &s.keys, sizeof s.keys, "the keys" },
                                        { kTripsAt, s.trips.data(), sizeof s.trips, "the trip objects" } });
}

const SettingsEvent kHandlerEvents[] = { handlerEvent };

constexpr SettingsSaid kHandlerSaid[] = { { 70, "controllers" }, { 71, "key-table routines" }, { 72, "notifies" } };
} // namespace

const SettingsPartRow kHandlerPartRow { 16, &SettingsCounts::handlerEvents, &SettingsCounts::handlerCalls,
                                        settingsListsCaseOf<kHandlerEvents, true>, nullptr, hookHandlers,
                                        "number handlers", kHandlerSaid };
#endif
