#pragma once
// xt_vc2settings' routines on the world the parameters' messages reach (the eleventh part's, v297; shared with the
// twelfth from v298): rom.4's routine and the port's on Vc2PartWorld, the nodes' lists written into rom.4's memory
// first; then the keys' span, the part objects, the class bytes and the lists; and a case of them.
// XT_VC2SETTINGS_MAIN only.
#include "Vc2PartNodesDraws.h"
#include "Vc2PartSettingsStates.h"
#include <type_traits>

// A case: the nodes noise, their object indices within rom.4's table and, with `Lists` (v298), their sources, channels
// and lists drawn as xt_vc2keys' second part draws them; G, the keys, the part objects and the class bytes noise (the
// indices within the table), the channel states' sources drawn, then the tables' and the events
template <const auto& Events, bool Lists>
std::string settingsWorldCaseOf(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    Vc2PartNodes& n = s.nodes;
    settingsNoise(n.nodes.data(), sizeof n.nodes, rng);
    for (Vc2PartNode& node : n.nodes) node.byteE = (uint8_t)(rng() % Vc2PartNodes::kObjects);
    if (Lists) vc2DrawNodeLists(n, rng);
    settingsNoise(&s.g, sizeof s.g, rng);
    settingsNoise(&s.keys, sizeof s.keys, rng);
    settingsNoise(n.objects.data(), sizeof n.objects, rng);
    for (Vc2PartObject& o : n.objects) o.index = (uint8_t)(rng() % Vc2PartNodes::kObjects);
    settingsNoise(n.classes.data(), sizeof n.classes, rng);
    s.fw.cpu.write(Vc2Keys::kTablesAt, &s.keys, sizeof s.keys);
    s.fw.cpu.write(Vc2PartNodes::kObjectsAt, n.objects.data(), sizeof n.objects);
    s.fw.cpu.write(Vc2PartNodes::kClassesAt, n.classes.data(), sizeof n.classes);
    settingsDrawStates(s, rng);
    return settingsCaseOf<Events, sizeof(Vc2PartRows), 5>(s, rng, events, calls);
}

// A routine on both sides with the world it reaches, the port's `mine(w)` giving the answer when the routine has one
template <class Mine>
std::string settingsWorldOn(SettingsSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                            Mine mine) {
    SettingsPortStates states { s.fw.cpu };
    Vc2PartWorld w { s.rows, s.g, s.setup, s.nodes, s.keys, states, s.outs, s.outs };
    Vc2PartNodes& n = s.nodes;
    s.fw.cpu.write(Vc2PartNodes::kListsAt, n.lists.data(), sizeof n.lists);
    const std::string what = settingsOnBoth(s, r, args, [&] {
        if constexpr (std::is_void_v<std::invoke_result_t<Mine, Vc2PartWorld&>>) {
            mine(w);
            return 0u;
        } else {
            return (uint32_t)mine(w);
        }
    });
    if (!what.empty()) return what;
    return vc2RegionsDifference(s.fw, { { Vc2Keys::kTablesAt, &s.keys, sizeof s.keys, "the keys" },
                                        { Vc2PartNodes::kObjectsAt, n.objects.data(), sizeof n.objects, "objects" },
                                        { Vc2PartNodes::kClassesAt, n.classes.data(), sizeof n.classes, "classes" },
                                        { Vc2PartNodes::kListsAt, n.lists.data(), sizeof n.lists, "lists" } });
}
