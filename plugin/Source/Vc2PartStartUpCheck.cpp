// xt_vc2settings' eighteenth part (v304): the part's start-up, vc2PartStartUp, against rom.4's own 0x89392ab0, on the
// seed seventeen after the run's.  Nothing more hooked: the routines under it rom.4's own (the part's init, the
// voices' and lists' inits, 0x8939b200, the static M's part init, acosf) but the static M's two calls out, which the
// ninth part hooked.  Each case every table noise, then 16 events, each the start-up on noise drawn first in all it
// reaches: the nodes, their lists, class bytes and part objects, the keys, the number objects and their table, the
// voices with their heads, globals and age word, the reload objects, the static M and the start-up's own words.
// After each the tables, those words, the calls and the answer.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"
#include "Vc2PartVoicesTables.h"
#include <vector>

namespace {
using W = Vc2PartStartWords;
using V = Vc2PartVoices;
constexpr uint32_t kStartUp = 0x89392ab0;   // [FW]

// The port's words the start-up reaches beyond the tables, where rom.4 holds them: written there before the event,
// compared after
std::vector<Vc2Region> startRegions(SettingsSides& s) {
    Vc2PartNodes& n = s.nodes;
    Vc2PartNumbers& p = s.numbers;
    V& v = s.voices;
    W& w = s.startWords;
    std::vector<Vc2Region> r {
        { Vc2PartNodes::kNodesAt, n.nodes.data(), sizeof n.nodes, "the nodes" },
        { Vc2PartNodes::kListsAt, n.lists.data(), sizeof n.lists, "the lists" },
        { Vc2PartNodes::kClassesAt, n.classes.data(), sizeof n.classes, "the class bytes" },
        { Vc2PartNodes::kObjectsAt, n.objects.data(), sizeof n.objects, "the part objects" },
        { Vc2Keys::kTablesAt, &s.keys, sizeof s.keys, "the keys" },
        { Vc2PartNumbers::kObjectsAt, p.objects.data(), sizeof p.objects, "the number objects" },
        { Vc2PartNumbers::kTableAt, p.table.data(), sizeof p.table, "their table" },
        { V::kHeadsAt, v.heads.data(), sizeof v.heads, "the voices' heads" },
        { V::kGlobalsAt, &v.g, sizeof v.g, "the voices' globals" },
        { V::kAgesAt, &v.ages, sizeof v.ages, "the next note's age" },
        { Vc2PartReloads::kAt, s.reloads.objects.data(), sizeof s.reloads.objects, "the reload objects" },
        { Vc2StaticMatrix::kAt, &s.staticM, sizeof s.staticM, "the static M" },
        { W::kFloatsAt, w.floats.data(), sizeof w.floats, "the routing object's floats" },
        { W::kRemainAt, &w.remain, sizeof w.remain, "the settings object's +0x14" } };
    for (int k = 0; k < V::kVoices; ++k)
        r.push_back({ v.addressOf(k), &v.voices[(size_t)k], sizeof(Vc2Voice), "a voice" });
    return r;
}

// The start-up's own bytes, where rom.4 holds them
std::array<std::pair<uint32_t, uint8_t>, 4> startBytes(const W& w) {
    return { { { W::kRoutingAt, w.routing[0] }, { W::kRoutingAt + 1, w.routing[1] },
               { W::kAfterAt, (uint8_t)w.after[0] }, { W::kAfterAt + 1, (uint8_t)w.after[1] } } };
}

// Noise in all the start-up reaches, the same on both sides
void drawStart(SettingsSides& s, std::mt19937& rng) {
    settingsNodesKeysNoise(s, rng);
    settingsNoise(s.nodes.classes.data(), sizeof s.nodes.classes, rng);
    settingsNoise(&s.numbers, sizeof s.numbers, rng);
    settingsNoise(&s.voices, sizeof s.voices, rng);
    settingsNoise(s.reloads.objects.data(), sizeof s.reloads.objects, rng);
    settingsNoise(&s.staticM, sizeof s.staticM, rng);
    settingsNoise(&s.startWords, sizeof s.startWords, rng);
    for (const Vc2Region& r : startRegions(s)) s.fw.cpu.write(r.at, r.mine, r.bytes);
    for (const auto& [at, b] : startBytes(s.startWords)) s.fw.cpu.wr8(at, b);
}

// What differs in the words the start-up reaches beyond the tables
std::string startWordsDifference(SettingsSides& s) {
    for (const Vc2Region& r : startRegions(s)) {
        std::string what = vc2FirstDifference(s.fw, r.at, r.mine, r.bytes, r.what);
        if (!what.empty()) return what;
    }
    return settingsBytesDifference(s.fw.cpu, startBytes(s.startWords));
}

// The start-up on both sides
std::string startUpEvent(SettingsSides& s, std::mt19937& rng) {
    drawStart(s, rng);
    ++s.n.byEvent[73];
    const std::string what = settingsOnBoth(s, { kStartUp, "the start-up", nullptr }, {}, [&] {
        Vc2PartInitWorld init { s.rows, s.nodes, s.keys, s.numbers };
        Vc2PartStartWorld w { init, s.startWords, s.voices, s.reloads, s.staticM, s.outs };
        vc2PartStartUp(w);
        return 0u;
    });
    return what.empty() ? startWordsDifference(s) : what;
}

const SettingsEvent kStartEvents[] = { startUpEvent };

std::string startUpCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    return settingsCaseOf<kStartEvents, sizeof(Vc2PartRows), 5, 16>(s, rng, events, calls);
}

constexpr SettingsSaid kStartSaid[] = { { 73, "start-ups" } };
} // namespace

const SettingsPartRow kStartPartRow { 17, &SettingsCounts::startEvents, &SettingsCounts::startCalls, startUpCase,
                                      nullptr, nullptr, "start-up", kStartSaid };
#endif
