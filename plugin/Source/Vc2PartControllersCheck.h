#pragma once
// xt_vc2settings' controllers' port side (the fifteenth part's, v301; shared with the sixteenth from v302): the
// controllers' calls out, each logged as rom.4's hooks log theirs, a word's byte and the part table read where rom.4
// reads them, the trip objects the check's own; the number's virtual logged, or (v302) handed to `number`.
// XT_VC2SETTINGS_MAIN only.
#include "Vc2PartSettingsCheck.h"
#include <functional>

// [FW] the controller, the part's controller, the part table, the trip object's two routines
inline constexpr uint32_t kController = 0x89392f90, kPartController = 0x89394440, kPartTableAt = 0x8993d040;
inline constexpr uint32_t kTripSetAt = 0x893b0610, kTripMoveAt = 0x893b0460;
// The check's own: the trip objects' fronts the part table points at
inline constexpr uint32_t kTripsAt = 0x8C042000;

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
