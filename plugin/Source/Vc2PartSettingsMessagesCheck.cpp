// xt_vc2settings' eleventh part (v297): the parameters' messages, 0x89392890 by kind and the part's 0x89392830, with
// the held note's switch 0x89391500 and a node's class 0x89394370 alone, against rom.4's, on the seed ten after the
// run's.  Each case every table, the nodes (each one's object index 0…15), G, the keys (v276's span), the part objects
// (an index 0…15) and the class bytes noise, and G's channel word's sources drawn.  Every routine they reach is rom.4's
// own (the dispatchers, the Matrix's settings, the system readers, the Category, a node's class install) but the reload
// task's request 0x8939af40 and run 0x8939af10, the receive channel 0x89393fb0 and the routing's reset 0x89393e30
// (these two answering as drawn), every voice's truncation 0x89391120 and a node's voices' stop 0x89390f70, hooked to
// log.  A message's two bytes in Vc2Fw's RAM, its kind 0…15, its second byte 0 half the time.  After each the tables,
// the keys' span, the nodes, the part objects, the class bytes and the calls; no answer is compared (the held note's
// switch's is 1, the others' their last callee's).  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsStates.h"

namespace {
constexpr uint32_t kMessages = 0x89392890, kPartMessages = 0x89392830, kHeldSwitch = 0x89391500;   // [FW]
constexpr uint32_t kNodeClassAt = 0x89394370, kReloadObjects = 0x89996120, kReloadBytes = 24;       // [FW]
constexpr uint32_t kHeldSwitchObject = 0x899826fc;                                                  // [FW]
constexpr uint32_t kMessageAt = kVc2Args, kKinds = 16, kObjects = 16;
// [FW] Auto Note's switch's second read object's word under 0x89392830 and the Category: 12, 44 and 32 bytes down
constexpr uint32_t kLeftoverWordAt = kVc2Sp - 76;
constexpr uint32_t kSpanBytes = sizeof(Vc2Keys);

// [FW] the callees and their calls as the log has them; the receive channel and the routing answer as drawn
SettingsCall requested(Sh4& c) { return { kOutReloadRequest, 0, 0, 0, (int32_t)c.r[5] }; }
SettingsCall ran(Sh4& c) { return { kOutReloadRun, 0, 0, 0, (int32_t)((c.r[4] - kReloadObjects) / kReloadBytes) }; }
SettingsCall received(Sh4&) { return { kOutReceive, 0, 0, 0, 0 }; }
SettingsCall routed(Sh4& c) { return { kOutRouting, 0, 0, 0, (int32_t)c.r[5] }; }
SettingsCall truncatedAll(Sh4&) { return { kOutTruncateAll, 0, 0, 0, 0 }; }
SettingsCall stopped(Sh4& c) { return { kOutStopVoices, 0, 0, 0, (int32_t)c.r[4] }; }
uint32_t receiveAnswer(SettingsSides& s) { return s.outs.answers[0] ? 1u : 0u; }
uint32_t routingAnswer(SettingsSides& s) { return s.outs.answers[1] ? 1u : 0u; }
constexpr SettingsCallee kParameterCallees[] = { { 0x8939af40, requested }, { 0x8939af10, ran },
                                                 { 0x89393fb0, received, receiveAnswer },
                                                 { 0x89393e30, routed, routingAnswer },
                                                 { 0x89391120, truncatedAll }, { 0x89390f70, stopped } };

void hookParameterCalls(SettingsSides& s) {
    s.rom.store.narrow = kMatrixNarrow;
    settingsHookCallees(s, kParameterCallees);
}

// A routine on both sides with the world it reaches; then the keys' span, the part objects and the class bytes
template <class Mine>
std::string messageOn(SettingsSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args, Mine mine) {
    SettingsPortStates states { s.fw.cpu };
    Vc2PartWorld w { s.rows, s.g, s.setup, s.nodes, s.keys, states, s.outs, s.outs };
    const std::string what = settingsOnBoth(s, r, args, [&] {
        mine(w);
        return 0u;
    });
    if (!what.empty()) return what;
    const Vc2PartNodes& n = s.nodes;
    return vc2RegionsDifference(s.fw, { { Vc2Keys::kTablesAt, &s.keys, kSpanBytes, "the keys" },
                                        { Vc2PartNodes::kObjectsAt, n.objects.data(), sizeof n.objects, "objects" },
                                        { Vc2PartNodes::kClassesAt, n.classes.data(), sizeof n.classes, "classes" } });
}

// A message's two bytes: a kind, and the second byte 0 half the time (part 0, routing off), else any
std::array<uint8_t, 2> drawMessage(SettingsSides& s, std::mt19937& rng, uint32_t kinds) {
    const std::array<uint8_t, 2> m { (uint8_t)(rng() % kinds), (uint8_t)(rng() % 2 ? 0 : rng()) };
    s.fw.cpu.write(kMessageAt, m.data(), m.size());
    for (bool& a : s.outs.answers) a = rng() % 2 == 0;
    return m;
}

std::string messageEvent(SettingsSides& s, std::mt19937& rng) {
    const auto m = drawMessage(s, rng, kKinds);
    ++s.n.byEvent[43];
    return messageOn(s, { kMessages, "the parameters' message", nullptr }, { 0, kMessageAt },
                     [&](Vc2PartWorld& w) { s.settings.parameters(w, m[0], m[1]); });
}

// The part's: kind 0 half the time; the leftover word Auto Note's switch may read under the Category given its handle
std::string partMessageEvent(SettingsSides& s, std::mt19937& rng) {
    const auto m = drawMessage(s, rng, rng() % 2 ? 1 : kKinds);
    s.fw.cpu.wr32(kLeftoverWordAt, kLeftoverHandle);
    ++s.n.byEvent[44];
    return messageOn(s, { kPartMessages, "the part's message", nullptr }, { 0, kMessageAt },
                     [&](Vc2PartWorld& w) { s.settings.partParameters(w, m[0], m[1]); });
}

// The held note's switch: its word 0 half the time, its flag 0, 1 or any
std::string heldSwitchEvent(SettingsSides& s, std::mt19937& rng) {
    if (rng() % 2 == 0) s.rows.heldNote = 0;
    s.fw.cpu.wr32(kHeldNoteAt, s.rows.heldNote);
    const std::array<uint32_t, 3> flags { 0, 1, (uint32_t)rng() };
    const uint32_t flag = flags[rng() % flags.size()];
    ++s.n.byEvent[45];
    return messageOn(s, { kHeldSwitch, "the held note's switch", nullptr }, { kHeldSwitchObject, flag },
                     [&](Vc2PartWorld& w) { s.settings.heldNoteSwitch(w.t, flag != 0, w.out); });
}

// A node's class by its flag, on a drawn part's node of source 1
std::string nodeClassEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts);
    const auto flag = (int32_t)(rng() % 2 ? 0 : rng());
    ++s.n.byEvent[46];
    return messageOn(s, { kNodeClassAt, "a node's class", nullptr }, { 0, (uint32_t)part, (uint32_t)flag },
                     [&](Vc2PartWorld& w) { s.settings.nodeClass(w, part, flag); });
}

const SettingsEvent kMessageEvents[] = { messageEvent, messageEvent, partMessageEvent, heldSwitchEvent,
                                         nodeClassEvent };

// A case: the nodes, G, the keys, the part objects and the class bytes noise (the indices within rom.4's table), the
// channel states' sources drawn, then the tables' and the events
std::string messageCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    Vc2PartNodes& n = s.nodes;
    settingsNoise(n.nodes.data(), sizeof n.nodes, rng);
    for (Vc2PartNode& node : n.nodes) node.byteE = (uint8_t)(rng() % kObjects);
    settingsNoise(&s.g, sizeof s.g, rng);
    settingsNoise(&s.keys, kSpanBytes, rng);
    settingsNoise(n.objects.data(), sizeof n.objects, rng);
    for (Vc2PartObject& o : n.objects) o.index = (uint8_t)(rng() % kObjects);
    settingsNoise(n.classes.data(), sizeof n.classes, rng);
    s.fw.cpu.write(Vc2Keys::kTablesAt, &s.keys, kSpanBytes);
    s.fw.cpu.write(Vc2PartNodes::kObjectsAt, n.objects.data(), sizeof n.objects);
    s.fw.cpu.write(Vc2PartNodes::kClassesAt, n.classes.data(), sizeof n.classes);
    settingsDrawStates(s, rng);
    return settingsCaseOf<kMessageEvents, sizeof(Vc2PartRows), 5>(s, rng, events, calls);
}

void sayMessagePart(const SettingsCounts& n) {
    const auto& e = n.byEvent;
    std::printf("messages: %ld events: %ld by kind, %ld the part's, %ld held note switches, %ld node classes (%ld "
                "calls)\n", n.messageEvents, e[43], e[44], e[45], e[46], n.messageCalls);
}
} // namespace

const SettingsPartRow kMessagePartRow { 10, &SettingsCounts::messageEvents, &SettingsCounts::messageCalls, messageCase,
                                        sayMessagePart, hookParameterCalls };
#endif
