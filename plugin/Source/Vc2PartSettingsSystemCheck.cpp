// xt_vc2settings' tenth part (v296): the system's readers the parameters' messages run, Time Trip's assigns
// 0x893943f0, Patch Remain 0x89391520 and the voices' counts 0x89391550, and the budget 0x893937b0, against rom.4's,
// on the seed nine after the run's, every table and the words they write noise.  The reserve 0x893911b0 and the power
// offset's setter 0x89391260 hooked to log among the reads, the budget's setter by v294's hook; Time Trip Assign X and
// Y drawn 0…39 half the time (0 and 32 none), Number of Voices and Offset of Voice Number 0…11 (within the reader's
// holds), Patch Category 0…13; before the voices' counts each last read set half the time to what the store will
// answer, so a reader finds it unchanged.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"

namespace {
constexpr uint32_t kTrip = 0x893943f0, kRemain = 0x89391520, kCounts = 0x89391550, kBudget = 0x893937b0;   // [FW]
constexpr uint32_t kRemainObject = 0x899826fc;   // [FW] what the part's handler gives 0x89391520, Patch Remain at +1
constexpr int16_t kCountIds[] = { 0x52d, 0x52e, 0x530, 0x531 };   // [FW] the voices' counts' IDs, in their order
constexpr SettingsNarrow kSystemNarrow[] = { { 0x93, 40, kHalfTheTime }, { 0x94, 40, kHalfTheTime },
                                             { 0x52d, 12, kHalfTheTime }, { 0x52e, 12, kHalfTheTime },
                                             { 0x12c, 14, kHalfTheTime } };

// [FW] the callees and their calls as the log has them
SettingsCall reserved(Sh4& c) { return { kOutReserve, 0, (int32_t)c.r[4], 0, (int32_t)c.r[5] }; }
SettingsCall powerOffset(Sh4& c) { return { kOutPowerOffset, 0, 0, 0, (int32_t)c.r[4] }; }
constexpr SettingsCallee kSystemCallees[] = { { 0x893911b0, reserved }, { 0x89391260, powerOffset } };

void hookSystem(SettingsSides& s) {
    s.rom.store.narrow = kSystemNarrow;
    settingsHookCallees(s, kSystemCallees);
}

// A reader that answers 1, given the object the part's handler gives Patch Remain's (Time Trip's doesn't read it)
template <uint32_t Routine, size_t Counted, void (Vc2PartSettings::*Reader)(Vc2PartRows&)>
std::string readerEvent(SettingsSides& s, std::mt19937&) {
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a system's reader", vc2R0 }, { kRemainObject }, [&] {
        (s.settings.*Reader)(s.rows);
        return 1u;
    });
}

// The voices' counts, each last read set half the time to the store's answer first
std::string countsEvent(SettingsSides& s, std::mt19937& rng) {
    for (size_t k = 0; k < std::size(kCountIds); ++k)
        if (rng() % 2 == 0)
            s.rows.voiceCounts[k] = (int16_t)s.rom.store.value({ (int32_t)Vc2ParamBlock::kSystemC, 0, 0, 0,
                                                                 kCountIds[k] });
    s.fw.cpu.write(kVoiceCountsAt, s.rows.voiceCounts.data(), sizeof s.rows.voiceCounts);
    ++s.n.byEvent[41];
    return settingsOnBoth(s, { kCounts, "the voices' counts", vc2R0 }, {}, [&] {
        s.settings.voiceCounts(s.rows, s.outs);
        return 1u;
    });
}

// The budget: rom.4's answers what its setter's hook does, so no answer is compared
std::string budgetEvent(SettingsSides& s, std::mt19937&) {
    ++s.n.byEvent[42];
    return settingsOnBoth(s, { kBudget, "the budget", nullptr }, {}, [&] {
        s.settings.budget(s.rows, s.outs);
        return 0u;
    });
}

const SettingsEvent kSystemEvents[] = { readerEvent<kTrip, 39, &Vc2PartSettings::tripAssigns>,
                                        readerEvent<kRemain, 40, &Vc2PartSettings::patchRemain>, countsEvent,
                                        budgetEvent };

constexpr SettingsSaid kSystemSaid[] = { { 39, "Time Trip assigns" }, { 40, "Patch Remains" },
                                         { 41, "voices' counts" }, { 42, "budgets" } };
} // namespace

const SettingsPartRow kSystemPartRow { 9, &SettingsCounts::systemEvents, &SettingsCounts::systemCalls,
                                       settingsCaseOf<kSystemEvents, sizeof(Vc2PartRows), 5>, nullptr, hookSystem,
                                       "system readers", kSystemSaid };
#endif
