// xt_vc2settings' thirteenth part (v299): the routing's reset 0x89393e30 against rom.4's, on the seed twelve after the
// run's; and kind 0's parameters' message (0x89392890) with rom.4's own routing reset and the port's
// (SettingsPortOuts::routed).  Each case every table (v299's words too), G, the keys, the part objects and the class
// bytes noise, the nodes noise with their sources, channels and lists drawn as the twelfth part's, G's channel word's
// sources drawn; before each event ChordMemory Switch's last read set to the store's answer half the time, the held
// note's word 0 half the time.  The store gives ChordMemory Switch 0 or 1 half the time, the receive channel's IDs as
// the twelfth part's and the Matrix Controls' Sens as the seventh's (kind 0's way runs the Matrix's settings).  Hooked
// to log: the controller, all notes off, the held note's event, every voice's truncation and a node's voices' stop; the
// eleventh part's hook on 0x89393e30 taken off.  After each the tables, the nodes, the lists, the keys' span, the calls,
// and the routing reset's answer.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsWorld.h"

namespace {
constexpr uint32_t kRouting = 0x89393e30, kMessages = 0x89392890;   // [FW]
constexpr uint32_t kMessageAt = kVc2Args;
constexpr uint8_t kRoutingKind = 0, kRoutingOn = 0x80;   // [FW] kind 0's message, its second byte's flag
constexpr int16_t kChordMemoryId = 55;                   // [FW] ChordMemory Switch, the system's fourth block
constexpr size_t kMatrixReceiveRows = 2;                 // kMatrixNarrow's Receive Switch and Channel, the receive's here
constexpr auto kRoutingNarrow = [] {
    std::array<SettingsNarrow, kReceiveNarrow.size() + kMatrixNarrow.size() - kMatrixReceiveRows + 1> n {};
    size_t k = 0;
    for (const SettingsNarrow& r : kReceiveNarrow) n[k++] = r;
    for (size_t j = kMatrixReceiveRows; j < kMatrixNarrow.size(); ++j) n[k++] = kMatrixNarrow[j];
    n[k] = { kChordMemoryId, 2, kHalfTheTime };
    return n;
}();

constexpr SettingsCallee kRoutingCallees[] = { kSettingsControllerCallee, kSettingsNotesOffCallee,
                                               kSettingsHeldOnCallee, kSettingsTruncateAllCallee,
                                               kSettingsStopCallee };
constexpr uint32_t kRoutingOwn[] = { kRouting };

void hookRouting(SettingsSides& s) {
    s.rom.store.narrow = kRoutingNarrow;
    settingsHookCallees(s, kRoutingCallees, kRoutingOwn);
}

// ChordMemory Switch's last read the store's answer half the time (no change heard), the held note's word 0 half the
// time, else 1 or any; after the event, the all notes off it heard
void drawRoutingWords(SettingsSides& s, std::mt19937& rng) {
    Vc2PartRows& t = s.rows;
    if (rng() % 2 == 0) t.chordMemory = s.rom.store.value({ kSystemDCall, 0, 0, 0, kChordMemoryId });
    s.fw.cpu.wr32(kChordMemoryAt, (uint32_t)t.chordMemory);
    const std::array<uint32_t, 4> words { 0, 0, 1, (uint32_t)rng() };
    t.heldNote = words[rng() % words.size()];
    s.fw.cpu.wr32(kHeldNoteAt, t.heldNote);
}

void countNotesOff(SettingsSides& s) {
    for (const SettingsCall& c : s.mine.log) s.n.byEvent[54] += c.block == kOutNotesOff;
}

// The routing reset alone: its flag 0 half the time, else the message's 0x80 or any word
std::string routingResetEvent(SettingsSides& s, std::mt19937& rng) {
    const std::array<uint32_t, 4> flags { 0, 0, kRoutingOn, (uint32_t)rng() };
    const uint32_t flag = flags[rng() % flags.size()];
    drawRoutingWords(s, rng);
    ++s.n.byEvent[52];
    const std::string what = settingsWorldOn(s, { kRouting, "the routing's reset", vc2R0 }, { 0, flag },
                                             [&](Vc2PartWorld& w) { return s.settings.routing(w, flag != 0); });
    countNotesOff(s);
    return what;
}

// Kind 0's message, its flag set half the time, the port's routing reset its own
std::string routingMessageEvent(SettingsSides& s, std::mt19937& rng) {
    const uint32_t b = rng();
    const std::array<uint8_t, 2> m { kRoutingKind, (uint8_t)(rng() % 2 ? b | kRoutingOn : b & ~kRoutingOn) };
    s.fw.cpu.write(kMessageAt, m.data(), m.size());
    drawRoutingWords(s, rng);
    ++s.n.byEvent[53];
    const std::string what = settingsWorldOn(s, { kMessages, "kind 0's message", nullptr }, { 0, kMessageAt },
                                             [&](Vc2PartWorld& w) {
        s.outs.routed = [&](bool on) { return s.settings.routing(w, on); };
        s.settings.parameters(w, m[0], m[1]);
        s.outs.routed = {};
    });
    countNotesOff(s);
    return what;
}

const SettingsEvent kRoutingEvents[] = { routingResetEvent, routingResetEvent, routingMessageEvent };
constexpr SettingsSaid kRoutingSaid[] = { { 52, "resets" }, { 53, "kind 0's messages" },
                                          { 54, "all notes off heard" } };
} // namespace

const SettingsPartRow kRoutingPartRow { 12, &SettingsCounts::routingEvents, &SettingsCounts::routingCalls,
                                        settingsWorldCaseOf<kRoutingEvents, true, sizeof(Vc2PartRows)>, nullptr,
                                        hookRouting, "routing reset", kRoutingSaid };
#endif
