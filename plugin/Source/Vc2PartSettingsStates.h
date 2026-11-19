#pragma once
// xt_vc2settings' channel states and the IDs the Matrix's set-up must be given narrow (the seventh part's, v292, shared
// with the eleventh from v297): G's channel word's two sources as the port reads them, where rom.4 does; their draws
// in rom.4's memory; and the store's narrow IDs for a part that runs rom.4's Matrix set-up.  XT_VC2SETTINGS_MAIN only.
#include "Vc2PartSettingsCheck.h"

// [FW] G's channel word's sources: the word, the source byte, the table (64 bytes a source, four sources)
inline constexpr uint32_t kUnreceivedAt = 0x8999738c, kStateSourceAt = 0x8950d6d9, kStatesAt = 0x8950d6e0;
inline constexpr uint32_t kSourceBytes = 64, kStateSources = 4;
// [FW] Part 1 Receive Switch and Receive Channel (system B's); the eight Matrix Controls' Sens 1 and 2 (five IDs each
// from 0x16a: Source, Dest 1, Sens 1, Dest 2, Sens 2), −128…127 always as v247's check draws them (rom.4's depth
// routine reads its tables by them)
inline constexpr int kControls = 8, kControlIds = 5, kSensFrom = -128;
inline constexpr int16_t kSens1 = 0x16c, kSens2 = 0x16e;
inline constexpr auto kMatrixNarrow = [] {
    std::array<SettingsNarrow, 2 + 2 * kControls> n { { { 71, 2, kHalfTheTime }, { 72, 16, kAlways } } };
    for (int k = 0; k < kControls; ++k)
        for (int j = 0; j < 2; ++j)
            n[(size_t)(2 + 2 * k + j)] = { (int16_t)((j == 0 ? kSens1 : kSens2) + kControlIds * k), 256, kAlways,
                                           kSensFrom };
    return n;
}();

// [FW] Part 1 Receive Switch and Channel, two IDs a part (the twelfth part's, v298; shared with the thirteenth from
// v299): each part's switch 0 or 1 half the time, its channel −1…17 always, both sides of 0…15 (a channel below −2
// reads past rom.4's lists, and −2's every-list moves are xt_vc2keys')
inline constexpr int16_t kReceiveSwitchId = 71, kReceiveChannelId = 72, kReceiveIds = 2;
inline constexpr uint32_t kReceiveChannelsDrawn = 19;
inline constexpr int32_t kReceiveChannelFrom = -1;
inline constexpr auto kReceiveNarrow = [] {
    std::array<SettingsNarrow, (size_t)(kReceiveIds * kSettingsParts)> n {};
    for (int p = 0; p < kSettingsParts; ++p) {
        n[(size_t)(kReceiveIds * p)] = { (int16_t)(kReceiveSwitchId + kReceiveIds * p), 2, kHalfTheTime };
        n[(size_t)(kReceiveIds * p + 1)] = { (int16_t)(kReceiveChannelId + kReceiveIds * p), kReceiveChannelsDrawn,
                                             kAlways, kReceiveChannelFrom };
    }
    return n;
}();

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

// A case's draws of the two sources in rom.4's memory: the source byte 0…3, the word any
inline void settingsDrawStates(SettingsSides& s, std::mt19937& rng) {
    s.fw.cpu.wr8(kStateSourceAt, (uint8_t)(rng() % kStateSources));
    s.fw.cpu.wr32(kUnreceivedAt, (uint32_t)rng());
}
