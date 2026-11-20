// xt_vc2settings' twentieth part (v307): the part's channel messages, Vc2PartChannel, against rom.4's own note-on
// 0x89392da0, note-off 0x89392e70 and the three that tell the listeners (0x893930a0, 0x893930c0, 0x893930e0) on its
// own key tables, nodes, lists and part objects, the pitch source 0x89989748 and Auto Note's On Switch 0x8993fe50, on
// the seed nineteen after the run's.  Hooked to log: the part objects' two virtuals of both classes (the held class's
// 0x89396a80 and 0x89396c90, the plain class's 0x89390540 and 0x89390550), v278's note-off over a key table
// 0x89390c00 and the listeners 0x89390ea0; the key tables' note-on and note-off rom.4's own.  Each case the tables
// noise, the nodes' lists drawn as xt_vc2keys' second part draws them, each node's part object one of the 16 and each
// part object's class either, the keys noise (each table's curve one of the eight); then 400 events: a note-on or
// note-off of a source 1…4, a channel, a key and a velocity (0 now and then), or one of the three; before each the
// pitch source's key −1 or a key, its count of other keys 0…2 or any, the On Switch 0, 1, 2 or any.  After each the
// tables, the keys' span, the pitch source, the calls.  Not drawn: a key or velocity past 127 (rom.4 reads past a
// table's counts and curve), a source but 1…4.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartControllersCheck.h"
#include "Vc2PartNodesDraws.h"

namespace {
using Args4 = std::array<uint32_t, 4>;
constexpr uint32_t kPitchAt = 0x89989748, kSwitchAt = 0x8993fe50;           // [FW]
constexpr uint32_t kHeldClass = 0x898cd3b8, kPlainClass = 0x898cd184;       // [FW] the part objects' two vtables
constexpr uint32_t kOnEntry = 0x0c, kOffEntry = 0x18, kEntryRoutine = 8;    // [FW] their two slots
constexpr uint32_t kObjectVtable = 4;                                       // [FW] a part object's vtable
constexpr int kCurves = 8, kKeys = 128;

SettingsCall objectOn(Sh4& c) {
    return { kOutObjectNote, (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[7], (int32_t)c.r[4] };
}
SettingsCall objectOnRoutine(Sh4& c) { return { kOutObjectRoutine, (int32_t)c.rd32(c.r[15]), 0, 0, (int32_t)c.pc }; }
SettingsCall objectOff(Sh4& c) { return { kOutObjectNote, (int32_t)c.r[5], (int32_t)c.r[6], 0, (int32_t)c.r[4] }; }
SettingsCall objectOffRoutine(Sh4& c) { return { kOutObjectRoutine, 0, 0, 0, (int32_t)c.pc }; }
SettingsCall tableReleased(Sh4& c) { return { kOutReleaseTable, 0, 0, 0, (int32_t)c.r[4] }; }
// [FW] the held class's note start and off, the plain class's, the release, the listeners
constexpr SettingsCallee kChannelCallees[] = {
    { 0x89396a80, objectOn, nullptr, objectOnRoutine },  { 0x89390540, objectOn, nullptr, objectOnRoutine },
    { 0x89396c90, objectOff, nullptr, objectOffRoutine }, { 0x89390550, objectOff, nullptr, objectOffRoutine },
    { 0x89390c00, tableReleased },                        kSettingsNotifyCallee,
};

void hookChannel(SettingsSides& s) { settingsHookCallees(s, kChannelCallees); }

// The port's parts: a part object's virtual by its vtable in rom.4's memory, logged as its hook logs it
class ChannelPortParts final : public Vc2ChannelParts {
public:
    explicit ChannelPortParts(SettingsSides& s) : s_(s) {}
    void objectNoteOn(uint32_t object, uint32_t table, uint32_t word, uint32_t key, uint32_t velocity) override {
        slotCalled(object, kOnEntry, { (int32_t)table, (int32_t)word, (int32_t)key }, velocity);
    }
    void objectNoteOff(uint32_t object, uint32_t table, uint32_t word) override {
        slotCalled(object, kOffEntry, { (int32_t)table, (int32_t)word, 0 }, 0);
    }
    void releaseTable(uint32_t table) override { logOut(s_.mine.log, kOutReleaseTable, table); }
    void notify(uint32_t state) override { logOut(s_.mine.log, kOutNotify, state); }

private:
    void slotCalled(uint32_t object, uint32_t entry, std::array<int32_t, 3> a, uint32_t more) {
        const uint32_t vtable = s_.fw.cpu.rd32(object + kObjectVtable);
        const uint32_t self = object + s_.fw.cpu.rd32(vtable + entry);
        const auto routine = (int32_t)s_.fw.cpu.rd32(vtable + entry + kEntryRoutine);
        s_.mine.log.push_back({ kOutObjectNote, a[0], a[1], a[2], (int32_t)self });
        s_.mine.log.push_back({ kOutObjectRoutine, (int32_t)more, 0, 0, routine });
    }
    SettingsSides& s_;
};

std::array<Vc2Region, 3> channelRegions(SettingsSides& s) {
    return { { { Vc2Keys::kTablesAt, &s.keys, sizeof s.keys, "the keys" },
               { kPitchAt, &s.pitch, sizeof s.pitch, "the pitch source" },
               { kSwitchAt, &s.autoNoteSwitch, sizeof s.autoNoteSwitch, "Auto Note's On Switch" } } };
}

// A routine on both sides after the pitch source and the On Switch drawn; what differs after, the calls counted
std::string channelOn(SettingsSides& s, std::mt19937& rng, const Vc2CheckRoutine& r, const Args4& a,
                      const std::function<void(Vc2ChannelWorld&)>& mine) {
    Vc2PitchSource& p = s.pitch;
    if (rng() % 2) p.note = rng() % 4 == 0 ? -1 : (int32_t)(rng() % kKeys);
    const int32_t counts[4] = { 0, 1, 2, (int32_t)(rng() % 64) };
    if (rng() % 2) p.others = counts[rng() % 4];
    const int32_t switches[4] = { 0, 1, 2, (int32_t)rng() };
    s.autoNoteSwitch = switches[rng() % 4];
    settingsWriteRegions(s, channelRegions(s));
    ChannelPortParts parts { s };
    Vc2ChannelWorld w { s.keys, s.nodes, s.pitch, s.autoNoteSwitch, parts };
    const std::string what = settingsOnBoth(s, r, { a[0], a[1], a[2], a[3] }, [&] {
        mine(w);
        return 0u;
    });
    settingsCountCalls(s, { { kOutObjectRoutine, 85 }, { kOutReleaseTable, 86 } });
    return what.empty() ? settingsRegionsDifference(s, channelRegions(s)) : what;
}

// A source 1…4, a channel, a key and a velocity (0 one time in eight)
Args4 drawChannelMessage(std::mt19937& rng) {
    return { 1 + rng() % 4, rng() % Vc2PartNodes::kChannels, rng() % kKeys, rng() % 8 == 0 ? 0 : rng() % kKeys };
}

template <bool On>
std::string noteEvent(SettingsSides& s, std::mt19937& rng) {
    const Args4 a = drawChannelMessage(rng);
    ++s.n.byEvent[On ? 82 : 83];
    const Vc2CheckRoutine r = On ? Vc2CheckRoutine { 0x89392da0, "the note-on", nullptr }
                                 : Vc2CheckRoutine { 0x89392e70, "the note-off", nullptr };
    return channelOn(s, rng, r, a, [&](Vc2ChannelWorld& w) {
        (On ? vc2ChannelNoteOn : vc2ChannelNoteOff)(w, (int32_t)a[0], (int32_t)a[1], a[2], a[3]);
    });
}

// [FW] the three that tell the listeners, one code
constexpr uint32_t kHeard[3] = { 0x893930a0, 0x893930c0, 0x893930e0 };

std::string heardEvent(SettingsSides& s, std::mt19937& rng) {
    const Args4 a = drawChannelMessage(rng);
    ++s.n.byEvent[84];
    return channelOn(s, rng, { kHeard[rng() % 3], "a message the listeners hear", nullptr }, a,
                     [&](Vc2ChannelWorld& w) { vc2ChannelHeard(w, (int32_t)a[0], (int32_t)a[1]); });
}

const SettingsEvent kChannelEvents[] = { noteEvent<true>, noteEvent<true>, noteEvent<false>, noteEvent<false>,
                                         heardEvent };

// A case: the nodes on lists, each one's part object one of the 16, each part object's class either, the keys noise
// with their curves one of the eight; the lists and the part objects into rom.4's memory; then the tables' and the
// events
std::string channelCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    Vc2PartNodes& n = s.nodes;
    settingsNoise(n.nodes.data(), sizeof n.nodes, rng);
    vc2DrawNodeLists(n, rng);
    for (Vc2PartNode& node : n.nodes)
        node.object = Vc2PartNodes::kObjectsAt + (uint32_t)(sizeof(Vc2PartObject) * (rng() % Vc2PartNodes::kObjects));
    for (Vc2PartObject& o : n.objects) o.vtable = rng() % 2 ? kHeldClass : kPlainClass;
    settingsNoise(&s.keys, sizeof s.keys, rng);
    for (Vc2KeyTable& t : s.keys.tables) t.curve = (uint8_t)(rng() % kCurves);
    s.fw.cpu.write(Vc2PartNodes::kListsAt, n.lists.data(), sizeof n.lists);
    s.fw.cpu.write(Vc2PartNodes::kObjectsAt, n.objects.data(), sizeof n.objects);
    return settingsCaseOf<kChannelEvents, sizeof(Vc2PartRows), 5>(s, rng, events, calls);
}

constexpr SettingsSaid kChannelSaid[] = { { 82, "note-ons" }, { 83, "note-offs" }, { 84, "messages heard" },
                                          { 85, "part objects' calls" }, { 86, "releases of a table" } };
} // namespace

const SettingsPartRow kChannelPartRow { 19, &SettingsCounts::channelEvents, &SettingsCounts::channelCalls, channelCase,
                                        nullptr, hookChannel, "channel messages", kChannelSaid };
#endif
