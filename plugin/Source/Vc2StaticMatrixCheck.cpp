// xt_vc2settings' ninth part (v295): the static M's static initializer 0x893a3020 and the part's init 0x893a2fe0
// against rom.4's, on the seed eight after the run's: before each event the static M (0x89997108, 0x290 bytes) noise
// on both sides; the channel state's set-up 0x89317a10 and the enqueue 0x89394fa0 hooked to log among the calls, the
// node's constructor 0x89394f30 rom.4's own; after each the tables as every part has them, the static M word for word
// and the calls.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"

namespace {
constexpr uint32_t kStaticInit = 0x893a3020, kPartInit = 0x893a2fe0;   // [FW]

// [FW] the callees and their calls as the log has them
SettingsCall stateInit(Sh4& c) { return { kOutChannelState, 0, 0, (int32_t)c.r[5], (int32_t)c.r[4] }; }
SettingsCall queued(Sh4& c) { return { kOutEnqueue, 0, (int32_t)c.r[4], 0, (int32_t)c.r[5] }; }
constexpr SettingsCallee kStaticCallees[] = { { 0x89317a10, stateInit }, { 0x89394fa0, queued } };

void hookStatic(SettingsSides& s) { settingsHookCallees(s, kStaticCallees); }

// An initializer on the static M given noise first, on both sides
template <uint32_t Routine, size_t Counted, auto Init>
std::string staticEvent(SettingsSides& s, std::mt19937& rng) {
    settingsNoise(&s.staticM, sizeof s.staticM, rng);
    s.fw.cpu.write(Vc2StaticMatrix::kAt, &s.staticM, sizeof s.staticM);
    ++s.n.byEvent[Counted];
    const std::string what = settingsOnBoth(s, { Routine, "a static M's initializer", nullptr }, {}, [&] {
        (s.staticM.*Init)(s.outs);
        return 0u;
    });
    if (!what.empty()) return what;
    return vc2RegionsDifference(s.fw, { { Vc2StaticMatrix::kAt, &s.staticM, sizeof s.staticM, "the static M" } });
}

const SettingsEvent kStaticEvents[] = { staticEvent<kStaticInit, 37, &Vc2StaticMatrix::staticInit>,
                                        staticEvent<kPartInit, 38, &Vc2StaticMatrix::partInit> };

void sayStaticPart(const SettingsCounts& n) {
    std::printf("static M: %ld events: %ld static initializers, %ld part inits (%ld calls)\n", n.staticEvents,
                n.byEvent[37], n.byEvent[38], n.staticCalls);
}
} // namespace

const SettingsPartRow kStaticPartRow { 8, &SettingsCounts::staticEvents, &SettingsCounts::staticCalls,
                                       settingsCaseOf<kStaticEvents, kV295Rows, 5>, sayStaticPart,
                                       hookStatic };
#endif
