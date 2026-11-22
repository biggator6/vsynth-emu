// xt_vc2settings' seventh part (v292): the dispatchers 0x8939b170 (on part 0, as rom.4's handler gives), 0x8939b0d0,
// 0x8939b110 and 0x8939b140, and G's channel word 0x893a5050, against rom.4's, on the seed six after the run's, every
// table, the nodes and G (0x89997398) noise.  Every reader rom.4's own, the Matrix's set-up too (0x893a51c0 and
// 0x893a50b0 against v247's on the same reads), the earlier parts' hooks on their callees as they were.  G's channel
// word's two sources drawn in rom.4's memory each case (the word 0x8999738c, the source byte 0x8950d6d9 within 0…3,
// the table 0x8950d6e0 as rom.4 holds it), and the port's read from there; Receive Switch drawn 0…1 half the time and
// Receive Channel 0…15 always (past the table rom.4 reads what lies around it).  The part's dispatcher's flip drawn
// anywhere in its reads, so that each reader is the one to give up now and then; the Matrix's settings alone too,
// whose answer the dispatcher can't show.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsStates.h"

namespace {
// About as many reads as the part's dispatcher makes with every reader through, and as its first three make (the
// common's flag 0 test after the bytes' and its own, 20 reads, is seldom reached otherwise)
constexpr uint32_t kPartReads = 7000, kFirstReads = 64;
constexpr uint32_t kMatrixReads = 128;   // a little past the Matrix's settings' reads (11 assignments, a record's 83)
constexpr uint32_t kHeldFlags = 5;

void narrowDispatch(SettingsSides& s) { s.rom.store.narrow = kMatrixNarrow; }

// The part's dispatcher on part 0, a flag set after a read drawn over all of its reads or, half the time, its first
// three readers'; the parts read whole counted
std::string partDispatchEvent(SettingsSides& s, std::mt19937& rng) {
    s.rom.flipAt = (int)(1 + rng() % (rng() % 2 ? kPartReads : kFirstReads));
    s.rom.flipFlag = (int)(rng() % kHeldFlags);
    ++s.n.byEvent[30];
    bool whole = false;
    const std::string what = settingsOnBoth(s, { 0x8939b170, "the part's dispatcher", vc2R0 }, { 0 }, [&] {
        whole = s.settings.readPart(s.rows, s.g, s.setup, 0, s.nodes, s.outs);
        return (uint32_t)whole;
    });
    s.n.wholeParts += whole;
    return what;
}

// [FW] the routines that answer their last reader's 1, given G (the dispatchers take nothing): where, what, the port's
struct SystemDispatch {
    uint32_t at;
    const char* name;
    void (*mine)(SettingsSides&);
};
void systemMine(SettingsSides& s) {
    SettingsPortStates states { s.fw.cpu };
    s.settings.readSystem(s.rows, s.g, states);
}
void assignsMine(SettingsSides& s) { s.settings.readAssigns(s.rows, s.g, s.setup); }
void calibrationMine(SettingsSides& s) { s.settings.readCalibration(s.rows); }
void channelMine(SettingsSides& s) {
    SettingsPortStates states { s.fw.cpu };
    s.settings.matrixChannel(s.g, states);
}
constexpr SystemDispatch kDispatches[] = { { 0x8939b0d0, "the system's dispatcher", systemMine },
                                           { 0x8939b110, "the assignments' dispatcher", assignsMine },
                                           { 0x8939b140, "the calibration's dispatcher", calibrationMine },
                                           { 0x893a5050, "G's channel word", channelMine } };

template <size_t K>
std::string dispatchEvent(SettingsSides& s, std::mt19937&) {
    const SystemDispatch& d = kDispatches[K];
    ++s.n.byEvent[31 + K];
    const std::string what = settingsOnBoth(s, { d.at, d.name, vc2R0 }, { kMatrixAt }, [&] {
        d.mine(s);
        return 1u;
    });
    for (const SettingsCall& c : s.mine.log) s.n.received += c.id == kMatrixNarrow[1].id;
    return what;
}

// The Matrix's settings alone (0x893a51c0) on a drawn part, a flag set within its reads: in the part's dispatcher its
// answer can't show, as the vocoder reader after it gives up on flag 0 before a read
std::string matrixSettingsAlone(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts);
    s.rom.flipAt = (int)(1 + rng() % kMatrixReads);
    ++s.n.byEvent[35];
    return settingsOnBoth(s, { 0x893a51c0, "the Matrix's settings", vc2R0 }, { kMatrixAt, (uint32_t)part },
                          [&] { return (uint32_t)s.setup.settings(s.g, part); });
}

const SettingsEvent kDispatchEvents[] = { partDispatchEvent, dispatchEvent<0>, dispatchEvent<1>, dispatchEvent<2>,
                                          dispatchEvent<3>, matrixSettingsAlone };

// A case: the nodes and G noise, G's channel word's sources drawn, then the tables' and the events
std::string dispatchCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    settingsNoise(s.nodes.nodes.data(), sizeof s.nodes.nodes, rng);
    settingsNoise(&s.g, sizeof s.g, rng);
    settingsDrawStates(s, rng);
    return settingsCaseOf<kDispatchEvents, kV293Rows, kHeldFlags>(s, rng, events, calls);
}

void sayDispatchPart(const SettingsCounts& n) {
    const auto& e = n.byEvent;
    std::printf("dispatchers: %ld events: %ld parts (%ld read whole), %ld systems, %ld assignments, %ld calibrations, "
                "%ld channel words, %ld Matrix settings; %ld of G's words from a channel's state (%ld reads)\n",
                n.dispatchEvents, e[30], n.wholeParts, e[31], e[32], e[33], e[34], e[35], n.received, n.dispatchCalls);
}
} // namespace

const SettingsPartRow kDispatchPartRow { 6, &SettingsCounts::dispatchEvents, &SettingsCounts::dispatchCalls,
                                         dispatchCase, sayDispatchPart, narrowDispatch };
#endif
