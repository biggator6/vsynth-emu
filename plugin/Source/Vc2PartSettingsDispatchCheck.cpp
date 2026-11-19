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
#include "Vc2PartSettingsCheck.h"

namespace {
// [FW] G's channel word's sources: the word, the source byte, the table (64 bytes a source, four sources)
constexpr uint32_t kUnreceivedAt = 0x8999738c, kStateSourceAt = 0x8950d6d9, kStatesAt = 0x8950d6e0;
constexpr uint32_t kSourceBytes = 64, kSources = 4;
// [FW] Part 1 Receive Switch and Receive Channel (system B's); the eight Matrix Controls' Sens 1 and 2 (five IDs each
// from 0x16a: Source, Dest 1, Sens 1, Dest 2, Sens 2), −128…127 always as v247's check draws them (rom.4's depth
// routine reads its tables by them)
constexpr int kControls = 8, kControlIds = 5, kSensFrom = -128;
constexpr int16_t kSens1 = 0x16c, kSens2 = 0x16e;
constexpr auto kDispatchNarrow = [] {
    std::array<SettingsNarrow, 2 + 2 * kControls> n { { { 71, 2, kHalfTheTime }, { 72, 16, kAlways } } };
    for (int k = 0; k < kControls; ++k)
        for (int j = 0; j < 2; ++j)
            n[(size_t)(2 + 2 * k + j)] = { (int16_t)((j == 0 ? kSens1 : kSens2) + kControlIds * k), 256, kAlways,
                                           kSensFrom };
    return n;
}();
// About as many reads as the part's dispatcher makes with every reader through, and as its first three make (the
// common's flag 0 test after the bytes' and its own, 20 reads, is seldom reached otherwise)
constexpr uint32_t kPartReads = 7000, kFirstReads = 64;
constexpr uint32_t kMatrixReads = 128;   // a little past the Matrix's settings' reads (11 assignments, a record's 83)
constexpr uint32_t kHeldFlags = 5;

// The port's two sources, read where rom.4 reads them
class SettingsPortStates final : public Vc2ChannelStates {
public:
    explicit SettingsPortStates(Sh4& cpu) : cpu_(cpu) {}
    uint32_t unreceived() override { return cpu_.rd32(kUnreceivedAt); }
    uint32_t receiving(int32_t channel) override {
        return cpu_.rd32(kStatesAt + kSourceBytes * cpu_.rd8(kStateSourceAt) + 4 * (uint32_t)channel);
    }

private:
    Sh4& cpu_;
};

void narrowDispatch(SettingsSides& s) { s.rom.store.narrow = kDispatchNarrow; }

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
    for (const SettingsCall& c : s.mine.log) s.n.received += c.id == kDispatchNarrow[1].id;
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
    s.fw.cpu.wr8(kStateSourceAt, (uint8_t)(rng() % kSources));
    s.fw.cpu.wr32(kUnreceivedAt, (uint32_t)rng());
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
