// xt_vc2settings' third part (v288): the OSC table's readers against rom.4's, the systems' 0x893a1000 and 0x893a1060
// and the part's 0x893a1410, on the seed two after the run's, every table noise; the OSC block's reader 0x893a04e0
// hooked to log its part, kind and OSC among the reads and answer from the store, mostly 1, now and then 0 or any
// word, a read the flip may follow as the getter's, the port's part reader given a block reader that does the same.
// Its fourth part (v289): the block reader rom.4's own, the hook off, on the seed three after the run's: the block
// reader and the part's reader on part 0, the Wave Number held half the time, the step counts compared.
// XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsEvents.h"

namespace {
using Settings = Vc2PartSettings;
// [FW] the OSC table's blocks, the Wave Number's ID and place (the check's own)
constexpr uint32_t kBlocksAt = 32, kOscKind = 0x100, kOscBytes = 0x80, kWaveAt = 0x3c;
constexpr int16_t kWaveId = 0x2fe, kOscIds = 52;
constexpr int kOscs = 2;

// The part's reader with the block reader hooked (v288's): rom.4's answers from the store, the port's the same
std::string hookedRowsEvent(SettingsSides& s, std::mt19937& rng) {
    return partEventWith<0x893a1410, kOscsAt, 23>(s, rng, [&](int part) {
        return s.settings.oscRows(s.rows, part, [&](int kind, int osc) {
            return s.mine.read({ kOscBlockCall, part, kind, osc, 0 }) != 0;
        });
    });
}

// [FW] the third part's events, counted after the second's
const SettingsEvent kOscEvents[] = {
    systemEvent<&Settings::oscTrailer, 0x893a1000, kOscsAt, 21>,
    systemEvent<&Settings::oscCalibration, 0x893a1060, kOscsAt, 22>,
    hookedRowsEvent,
};

// Half the time, part 0's block's Wave Number as the store will answer it, on both sides, so that it stays
void waveHeld(SettingsSides& s, std::mt19937& rng, int kind, int osc) {
    if (rng() % 2 == 0) return;
    const SettingsCall read { (int32_t)Vc2ParamBlock::kKindA, 0, kind, 0, (int16_t)(kWaveId + kOscIds * osc) };
    const auto wave = (uint16_t)s.rom.store.value(read);
    const uint32_t at = kBlocksAt + kOscKind * kind + kOscBytes * osc + kWaveAt;
    std::memcpy(s.rows.oscs.data() + at, &wave, sizeof wave);
    s.fw.cpu.wr16(kOscsAt + at, wave);
}

// rom.4's block reader on part 0, kind 0 one time in four
std::string blockEvent(SettingsSides& s, std::mt19937& rng) {
    const int kind = rng() % 4 == 0 ? 0 : (int)(rng() % kSettingsKinds), osc = (int)(rng() % kOscs);
    waveHeld(s, rng, kind, osc);
    ++s.n.byEvent[24];
    const Vc2CheckRoutine r { kOscBlock, "an OSC block's reader", vc2R0 };
    return settingsOnBoth(s, r, { kOscsAt, 0, (uint32_t)kind, (uint32_t)osc },
                          [&] { return (uint32_t)s.settings.oscBlock(s.rows, 0, kind, osc); });
}

// The part's reader on part 0 with rom.4's block reader, each block's Wave Number held half the time
std::string blockRowsEvent(SettingsSides& s, std::mt19937& rng) {
    for (int kind = 0; kind < kSettingsKinds; ++kind)
        for (int osc = 0; osc < kOscs; ++osc) waveHeld(s, rng, kind, osc);
    ++s.n.byEvent[25];
    return settingsOnBoth(s, { 0x893a1410, "a part's reader", vc2R0 }, { kOscsAt, 0 },
                          [&] { return (uint32_t)s.settings.oscs(s.rows, 0); });
}

// [FW] the fourth part's events
const SettingsEvent kBlockEvents[] = { blockEvent, blockRowsEvent };

void sayOscPart(const SettingsCounts& n) {
    std::printf("OSC table: %ld events: %ld trailers, %ld calibrations, %ld parts (%ld reads)\n", n.oscEvents,
                n.byEvent[21], n.byEvent[22], n.byEvent[23], n.oscCalls);
}

void sayBlockPart(const SettingsCounts& n) {
    std::printf("OSC blocks: %ld events: %ld blocks, %ld parts (%ld reads)\n", n.blockEvents, n.byEvent[24],
                n.byEvent[25], n.blockCalls);
}

void unhookBlock(SettingsSides& s) { s.fw.cpu.unhook(kOscBlock); }
} // namespace

// The third part's noise stops before the step counts (v289's), the fourth's after them (before v290's word), so
// their draws are as they were
constexpr size_t kV289Rows = offsetof(Vc2PartRows, steps) + sizeof(Vc2PartRows::steps);
const SettingsPartRow kOscPartRows[2] = {
    { 2, &SettingsCounts::oscEvents, &SettingsCounts::oscCalls,
      settingsCaseOf<kOscEvents, offsetof(Vc2PartRows, steps), 5>, sayOscPart },
    { 3, &SettingsCounts::blockEvents, &SettingsCounts::blockCalls,
      settingsCaseOf<kBlockEvents, kV289Rows, 5>, sayBlockPart, unhookBlock },
};
#endif
