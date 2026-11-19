// xt_vc2voices' third part (v278): the assigner's second part, the release path and the part node's note start and
// off, against rom.4's own routines on its own voices and keys, after the assigner's part on the seed after its.  Beyond
// the voices' and the assigner's hooks: the voice's +0x30 virtual of both classes, the parameters' message 0x8939b050
// and the OS clock 0x893018d0 (answering the ms an event draws) are hooked and logged.  The keys (v276's span) are the
// port's, written into rom.4's memory before each event and compared after it beside the voices' span.  The draws,
// beyond the assigner's: the voices' held bits, keys and note-on counts, some voices on the three key tables with a
// message for their part; the tables' sources, pedals, latched and held keys, note-off keys and counts; every glide
// aimed at one of the three, its flags 0 or 1, its clock below 2^30; four messages (their first byte a part 0…3) and
// four nodes (a channel 0…15); the parts' settings bytes.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2AssignReleaseCheck.h"
#include "Vc2CheckCalls.h"
#include <cstring>

namespace {
using V = Vc2PartVoices;
constexpr int kSteps = 400;
constexpr int32_t kClockMost = 1 << 30;
constexpr uint32_t kSettingOffsets[5] = { 0, 1, 2, 0x16, 0x18 };   // [FW] the bytes the routines read

uint32_t tableNumber(std::mt19937& rng) { return rng() % kReleaseTables; }

// rom.4's side: the key-off virtual of both classes, the parameters' message and the clock
void releaseInstallRom(ReleaseSides& r) {
    VoicesSides& s = r.a.s;
    for (const uint32_t vtable : { V::kVoiceVtable, kVoicesNoteClass }) {
        const VoicesCall slot = voicesSlotCall(s.fw, kVoicesKeyOff, 0, vtable, kAssignKeyOffEntry);
        s.fw.cpu.hook(slot.routine, [&s, slot](Sh4& c) {
            s.rom.push_back({ kVoicesKeyOff, slot.routine, c.r[4] });
            c.ret(0);
        });
    }
    s.fw.cpu.hook(kAssignReadAt, [&s](Sh4& c) {
        s.rom.push_back({ kVoicesParameters, kAssignReadAt, c.r[4] });
        c.ret(0);
    });
    s.fw.cpu.hook(kAssignClockAt, [&r](Sh4& c) {
        r.a.s.rom.push_back({ kVoicesClock, kAssignClockAt, 0 });
        c.ret((uint32_t)r.a.parts.ms);
    });
}

std::string keysDiffer(ReleaseSides& r) {
    std::vector<uint8_t> theirs(sizeof r.keys);
    r.a.s.fw.cpu.read(Vc2Keys::kTablesAt, theirs.data(), theirs.size());
    const auto* ours = reinterpret_cast<const uint8_t*>(&r.keys);
    const auto [a, b] = std::mismatch(theirs.begin(), theirs.end(), ours);
    if (a == theirs.end()) return "";
    const uint32_t at = Vc2Keys::kTablesAt + (uint32_t)(a - theirs.begin());
    char t[96];
    std::snprintf(t, sizeof t, "the keys' byte 0x%08x: %02x, against %02x", at, *b, *a);
    return t;
}

// A glide as the routines meet it: aimed at one of the three tables, its flags 0 or 1, its clock below 2^30
void releaseGlide(Vc2KeyGlide& g, std::mt19937& rng) {
    g.table = Vc2Keys::tableAt(0, tableNumber(rng));
    g.lastMs = (int32_t)(rng() % kClockMost);
    for (uint8_t* flag : { &g.prevPolyHeld, &g.lastPolyHeld, &g.prevMonoHeld, &g.lastMonoHeld })
        *flag = (uint8_t)(rng() % 2);
}

// A table's words the routines decide by
void releaseTableWords(Vc2KeyTable& t, std::mt19937& rng) {
    const uint8_t pedals[5] = { 0, 63, 64, 127, (uint8_t)rng() };
    t.source = (uint8_t)(rng() % Vc2Keys::kSources);
    t.hold = pedals[rng() % 5];
    t.sostenuto = pedals[rng() % 5];
    t.latched[rng() % 8] = (uint16_t)rng();
    t.held[rng() % 8] = rng() % 2 ? (uint16_t)rng() : 0;
    if (rng() % 3 == 0) t.held.fill(0);
    if (rng() % 6 == 0) t.held = { 1, 0, 0, 0, 0, 0, 0, 0 };   // key 0 alone, where the queries' bounds show
    t.offKey = (int8_t)(rng() % 128);
    t.offs[(uint8_t)t.offKey] = (uint8_t)(rng() % 4);
}

// A voice's words the release path decides by; half the time on one of the tables, with a message for its part and,
// half of those, the table's note-off key
void releaseVoiceWords(ReleaseSides& r, uint32_t voice, std::mt19937& rng) {
    Vc2VoiceNode& v = r.a.s.port.at(voice);
    v.held = (uint8_t)(rng() % 8);
    v.byte42 = (uint8_t)(rng() % 128);
    v.byte2e = (uint8_t)(rng() % 4);
    if (rng() % 2) return;
    const uint32_t table = Vc2Keys::tableAt(0, tableNumber(rng));
    v.channel = table;
    v.part = releaseMessage(rng);
    if (rng() % 2) v.byte42 = (uint8_t)r.keys.table(table).offKey;
}

void steerRelease(ReleaseSides& r, std::mt19937& rng, bool all) {
    assignSteer(r.a, rng, all);
    for (int i = 0; i < V::kVoices; ++i)
        if (all || rng() % 4 == 0) releaseVoiceWords(r, r.a.s.port.addressOf(i), rng);
    for (uint32_t k = 0; k < (uint32_t)kReleaseTables; ++k)
        if (all || rng() % 4 == 0) releaseTableWords(r.keys.table(Vc2Keys::tableAt(0, k)), rng);
    for (Vc2KeyGlide& g : r.keys.glides)
        if (all || rng() % 16 == 0) releaseGlide(g, rng);
    for (uint32_t k = 0; k < (uint32_t)kReleaseObjects; ++k) {
        r.a.s.fw.cpu.wr8(kReleaseMessagesAt + kReleaseObjectBytes * k, (uint8_t)(rng() % kReleaseParts));
        r.a.s.fw.cpu.wr8(kReleaseNodesAt + kReleaseObjectBytes * k, (uint8_t)(rng() % Vc2Keys::kChannels));
    }
    for (uint32_t part = 0; part < (uint32_t)kReleaseParts; ++part)
        for (const uint32_t offset : kSettingOffsets)
            if (all || rng() % 8 == 0)
                r.a.s.fw.cpu.wr8(kAssignSettingsAt + kAssignSettingBytes * part + offset,
                                 offset == 0 ? (uint8_t)(rng() % 25 - 12) : (uint8_t)(rng() % 3 == 0));
    r.a.parts.ms = (int32_t)(rng() % kClockMost);
}

std::string releaseCase(ReleaseSides& r, std::mt19937& rng) {
    std::string what = assignStart(r.a, rng);
    std::vector<uint32_t> noise(sizeof r.keys / 4);
    for (uint32_t& w : noise) w = (uint32_t)rng();
    std::memcpy(&r.keys, noise.data(), sizeof r.keys);
    const std::span<const ReleaseEvent> events = releaseEvents();
    for (int k = 0; k < kSteps && what.empty(); ++k, ++r.n.events) {
        steerRelease(r, rng, k == 0);
        voicesPrepare(r.a.s, (uint32_t)rng());
        what = events[rng() % events.size()](r, rng);
        r.n.keyOffs += vc2CountCalls(r.a.s.mine, kVoicesKeyOff);
        r.n.builds += vc2CountCalls(r.a.s.mine, kVoicesPrepare);
        r.n.retunes += vc2CountCalls(r.a.s.mine, kVoicesRetune);
    }
    return what;
}

void sayReleaseCounts(const ReleaseCounts& n) {
    const auto& e = n.byEvent;
    std::printf("release: %ld events: %ld releases, %ld and %ld lifts, %ld note-offs, %ld and %ld pedals off, %ld "
                "truncations, %ld and %ld releases of a table, %ld node note starts and %ld offs (%ld key-offs, %ld "
                "voices prepared, %ld retunes)\n", n.events, e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8],
                e[9], e[10], n.keyOffs, n.builds, n.retunes);
}
} // namespace

std::string releaseOnBoth(ReleaseSides& r, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                          const std::function<void()>& mine) {
    r.a.s.fw.cpu.write(Vc2Keys::kTablesAt, &r.keys, sizeof r.keys);
    const auto ours = [&] {
        mine();
        return 0u;
    };
    const std::string what = vc2BothAnswer(r.a.s.fw, routine, args, ours, [&] {
        const std::string voices = voicesDifference(r.a.s);
        return voices.empty() ? keysDiffer(r) : voices;
    });
    return what.empty() ? what : std::string(routine.name) + ": " + what;
}

uint32_t releaseTable(std::mt19937& rng) { return Vc2Keys::tableAt(0, tableNumber(rng)); }

uint32_t releaseMessage(std::mt19937& rng) {
    return kReleaseMessagesAt + kReleaseObjectBytes * (rng() % kReleaseObjects);
}

uint32_t releaseNode(std::mt19937& rng) { return kReleaseNodesAt + kReleaseObjectBytes * (rng() % kReleaseObjects); }

long vc2AssignReleasePart(VoicesSides& s, const Vc2Run& run) {
    const auto r = std::make_unique<ReleaseSides>(ReleaseSides { { s, {}, AssignPortParts(s) } });
    releaseInstallRom(*r);
    Vc2Tally tally { "XT_VC2VOICES_FIRST", "XT_VC2VOICES_STOP" };
    Vc2Run own = run;
    own.seed = run.seed + 2;
    vc2RunCases(own, tally, "", r->n.events, [&](std::mt19937& rng, bool) { return releaseCase(*r, rng); });
    sayReleaseCounts(r->n);
    return tally.different;
}
#endif
