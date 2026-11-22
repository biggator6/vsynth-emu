#pragma once
// xt_vc2settings' controllers' port side (the fifteenth part's, v301; shared with the sixteenth from v302): the
// controllers' calls out, each logged as rom.4's hooks log theirs, a word's byte and the part table read where rom.4
// reads them, the trip objects the check's own; the number's virtual logged, or (v302) handed to `number`.
// XT_VC2SETTINGS_MAIN only.
#include "Vc2PartNodesDraws.h"
#include "Vc2PartSettingsCheck.h"
#include <functional>

// [FW] the controller, the part's controller, the part table, the trip object's two routines
inline constexpr uint32_t kController = 0x89392f90, kPartController = 0x89394440, kPartTableAt = 0x8993d040;
inline constexpr uint32_t kTripSetAt = 0x893b0610, kTripMoveAt = 0x893b0460;
// The check's own: the trip objects' fronts the part table points at
inline constexpr uint32_t kTripsAt = 0x8C042000;
// [FW] the number classes' handlers, the check's own list (the sixteenth part's hooks, v302; the seventeenth takes
// them off, v303)
inline constexpr uint32_t kNumberHandlers[] = { 0x893938d0, 0x89393920, 0x89393950, 0x89393980, 0x89393a10,
                                                0x89393a70, 0x89393ac0, 0x89393ba0, 0x89393c10, 0x89393c80,
                                                0x89393ce0, 0x89393d90 };

// The check's own: each node's word's byte, its part (the fifteenth part's, v301; the seventeenth's too from v303)
inline constexpr uint32_t kNodeBytesAt = 0x8C043000;

// A case on the nodes' lists (the fifteenth part's, v301; the seventeenth's from v303): the nodes noise, their lists
// drawn as xt_vc2keys' second part draws them, each one's word a byte of its own with a part 0…15 (with `Handlers`, its
// object index 0…15 drawn first, and the keys noise after), the trip objects noise; then the tables' and the events
template <const auto& Events, bool Handlers>
std::string settingsListsCaseOf(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    Vc2PartNodes& n = s.nodes;
    settingsNoise(n.nodes.data(), sizeof n.nodes, rng);
    vc2DrawNodeLists(n, rng);
    for (uint32_t k = 0; k < (uint32_t)kVc2Nodes; ++k) {
        if (Handlers) n.nodes[k].byteE = (uint8_t)(rng() % Vc2PartNodes::kObjects);
        n.nodes[k].word8 = kNodeBytesAt + k;
        s.fw.cpu.wr8(kNodeBytesAt + k, (uint8_t)(rng() % kSettingsParts));
    }
    if (Handlers) {
        settingsNoise(&s.keys, sizeof s.keys, rng);
        s.fw.cpu.write(Vc2Keys::kTablesAt, &s.keys, sizeof s.keys);
    }
    settingsNoise(s.trips.data(), sizeof s.trips, rng);
    return settingsCaseOf<Events, sizeof(Vc2PartRows), 5>(s, rng, events, calls);
}

// The port's calls of each code counted by an event's count (v303; the fifteenth part's loop until then)
inline void settingsCountCalls(SettingsSides& s, std::initializer_list<std::pair<int32_t, size_t>> counted) {
    for (const SettingsCall& c : s.mine.log)
        for (const auto& [code, at] : counted) s.n.byEvent[at] += c.block == code;
}

class ControllerPortParts final : public Vc2ControllerParts {
public:
    explicit ControllerPortParts(SettingsSides& s) : s_(s) {}
    std::function<void(int32_t, int, int32_t, int32_t)> number {};   // v302: the number's virtual, the port's own
    void numberChanged(int32_t n, int source, int32_t channel, int32_t value) override {
        if (number) return number(n, source, channel, value);
        s_.mine.log.push_back({ kOutNumber, source, channel, value, n });
    }
    uint8_t firstByte(uint32_t word) override { return s_.fw.cpu.rd8(word); }
    uint32_t tripAt(uint8_t part) override { return s_.fw.cpu.rd32(kPartTableAt + 4u * part); }
    Vc2TripFront& trip(uint32_t address) override { return s_.trips[(address - kTripsAt) / sizeof(Vc2TripFront)]; }
    void tripSet(uint32_t trip, int32_t on, int8_t x, int8_t y) override {
        s_.mine.log.push_back({ kOutTripSet, on, x, y, (int32_t)trip });
    }
    uint8_t tripMove(uint32_t trip, int8_t x, int8_t y) override {
        s_.mine.log.push_back({ kOutTripMove, 0, x, y, (int32_t)trip });
        return s_.tripAnswer;
    }

private:
    SettingsSides& s_;
};
