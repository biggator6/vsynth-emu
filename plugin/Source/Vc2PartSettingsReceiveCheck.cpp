// xt_vc2settings' twelfth part (v298): the receive channel, 0x89393fd0 for a drawn part and part 0's 0x89393fb0,
// against rom.4's, on the seed eleven after the run's; and the system's parameters' message (0x89392890, kind 4) with
// rom.4's own receive channel and the port's (SettingsPortOuts::receiving).  Each case every table, G, the keys (v276's
// span), the part objects and the class bytes noise, the nodes noise with their sources, channels and lists drawn as
// xt_vc2keys' second part draws them, G's channel word's sources drawn; before each event the held note's word 0 half
// the time, else 1 or any.  The store gives each part's Receive Switch 0 or 1 half the time and its Receive Channel −1…17 always (both
// sides of 0…15; a channel below −2 reads past rom.4's lists, and −2's every-list moves are xt_vc2keys'), the system's
// fourth block's set-up 0x893206f0 leaving a handle as the others'.  Hooked to log: the controller 0x89392f90, all
// notes off 0x89391e30, the held note's event 0x89391e50 and a node's voices' stop 0x89390f70; the eleventh part's hook
// on 0x89393fb0 taken off.  After each the tables, the nodes, the lists, the keys' span, the calls, and the receive
// routines' answer.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsWorld.h"

namespace {
constexpr uint32_t kReceiveChannel = 0x89393fd0, kReceive = 0x89393fb0, kMessages = 0x89392890;   // [FW]
constexpr uint32_t kMessageAt = kVc2Args;
constexpr uint8_t kSystemKind = 4;   // [FW] the system's parameters' message, whose way runs part 0's receive channel
// [FW] Part 1 Receive Switch and Channel, two IDs a part
constexpr int16_t kSwitchId = 71, kChannelId = 72, kIdsAPart = 2;
constexpr uint32_t kChannelsDrawn = 19;
constexpr int32_t kChannelFrom = -1;
constexpr auto kReceiveNarrow = [] {
    std::array<SettingsNarrow, (size_t)(kIdsAPart * kSettingsParts)> n {};
    for (int p = 0; p < kSettingsParts; ++p) {
        n[(size_t)(kIdsAPart * p)] = { (int16_t)(kSwitchId + kIdsAPart * p), 2, kHalfTheTime };
        n[(size_t)(kIdsAPart * p + 1)] = { (int16_t)(kChannelId + kIdsAPart * p), kChannelsDrawn, kAlways,
                                           kChannelFrom };
    }
    return n;
}();

SettingsCall heldOn(Sh4&) { return { kOutHeldNoteOn, 0, 0, 0, 0 }; }
constexpr SettingsCallee kReceiveCallees[] = { kSettingsControllerCallee, kSettingsNotesOffCallee,
                                               { 0x89391e50, heldOn }, kSettingsStopCallee };
constexpr uint32_t kReceiveOwn[] = { kReceive };

void hookReceive(SettingsSides& s) {
    s.rom.store.narrow = kReceiveNarrow;
    settingsHookCallees(s, kReceiveCallees, kReceiveOwn);
}

// The held note's word 0 half the time, else 1 or any word; after the event, which calls out it heard
void drawHeldNote(SettingsSides& s, std::mt19937& rng) {
    const std::array<uint32_t, 4> words { 0, 0, 1, (uint32_t)rng() };
    s.rows.heldNote = words[rng() % words.size()];
    s.fw.cpu.wr32(kHeldNoteAt, s.rows.heldNote);
}

void countHeard(SettingsSides& s) {
    for (const SettingsCall& c : s.mine.log) {
        s.n.byEvent[50] += c.block == kOutController;
        s.n.byEvent[51] += c.block == kOutHeldNoteOn;
    }
}

// A part's receive channel: part 0 half the time
std::string channelEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = rng() % 2 ? 0 : (int)(rng() % kSettingsParts);
    drawHeldNote(s, rng);
    ++s.n.byEvent[47];
    const std::string what = settingsWorldOn(s, { kReceiveChannel, "a part's receive channel", vc2R0 },
                                             { 0, (uint32_t)part },
                                             [&](Vc2PartWorld& w) { return s.settings.receiveChannel(w, part); });
    countHeard(s);
    return what;
}

std::string partZeroEvent(SettingsSides& s, std::mt19937& rng) {
    drawHeldNote(s, rng);
    ++s.n.byEvent[48];
    const std::string what = settingsWorldOn(s, { kReceive, "part 0's receive channel", vc2R0 }, {},
                                             [&](Vc2PartWorld& w) { return s.settings.receive(w); });
    countHeard(s);
    return what;
}

// The system's message, its second byte 0 half the time, the port's receive channel its own
std::string systemMessageEvent(SettingsSides& s, std::mt19937& rng) {
    const std::array<uint8_t, 2> m { kSystemKind, (uint8_t)(rng() % 2 ? 0 : rng()) };
    s.fw.cpu.write(kMessageAt, m.data(), m.size());
    drawHeldNote(s, rng);
    ++s.n.byEvent[49];
    const std::string what = settingsWorldOn(s, { kMessages, "the system's message", nullptr }, { 0, kMessageAt },
                                             [&](Vc2PartWorld& w) {
        s.outs.receiving = [&] { return s.settings.receive(w); };
        s.settings.parameters(w, m[0], m[1]);
        s.outs.receiving = {};
    });
    countHeard(s);
    return what;
}

const SettingsEvent kReceiveEvents[] = { channelEvent, channelEvent, partZeroEvent, systemMessageEvent };
constexpr SettingsSaid kReceiveSaid[] = { { 47, "parts' channels" }, { 48, "part 0's" }, { 49, "system messages" },
                                          { 50, "controllers heard" }, { 51, "held notes on" } };
} // namespace

const SettingsPartRow kReceivePartRow { 11, &SettingsCounts::receiveEvents, &SettingsCounts::receiveCalls,
                                        settingsWorldCaseOf<kReceiveEvents, true>, nullptr, hookReceive,
                                        "receive channel", kReceiveSaid };
#endif
