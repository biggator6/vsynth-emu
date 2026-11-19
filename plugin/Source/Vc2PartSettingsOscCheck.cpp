// xt_vc2settings' third part (v288): the OSC table's readers against rom.4's, the systems' 0x893a1000 and 0x893a1060
// and the part's 0x893a1410, on the seed two after the run's, every table noise; the OSC block's reader 0x893a04e0
// hooked to log its part, kind and OSC among the reads and answer from the store, mostly 1, now and then 0 or any
// word, a read the flip may follow as the getter's.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"

namespace {
using Settings = Vc2PartSettings;

// [FW] the third part's events, counted after the second's
const SettingsEvent kOscEvents[] = {
    systemEvent<&Settings::oscTrailer, 0x893a1000, kOscsAt, 21>,
    systemEvent<&Settings::oscCalibration, 0x893a1060, kOscsAt, 22>,
    partEvent<&Settings::oscs, 0x893a1410, kOscsAt, 23>,
};
} // namespace

long settingsOscPart(SettingsSides& s, const Vc2Run& run) {
    SettingsCounts& n = s.n;
    const long different = vc2CheckPart(run, 2, { "XT_VC2SETTINGS_FIRST", "XT_VC2SETTINGS_STOP" }, n.oscEvents,
        [&](std::mt19937& rng) { return settingsCaseOf<kOscEvents, sizeof(Vc2PartRows), 5>(s, rng, n.oscEvents,
                                                                                        n.oscCalls); });
    const auto& e = n.byEvent;
    std::printf("OSC table: %ld events: %ld trailers, %ld calibrations, %ld parts (%ld reads)\n", n.oscEvents, e[21],
                e[22], e[23], n.oscCalls);
    return different;
}
#endif
