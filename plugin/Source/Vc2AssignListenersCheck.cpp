// xt_vc2voices' fifth part (v305): the listeners' run, Vc2Assigner::notify, against rom.4's own 0x89390ea0 on its own
// voices and keys, after the voice class's part on the seed after its.  The release part's sides, start and draws, and
// beyond them the +0x48 virtual of both classes (the note's change test 0x893972d0, which the fourth part ran as
// rom.4's own, and the plain voice's 0x89391720) hooked to log the voice and its +0x3c.  Before each event every voice
// put on one of the three key tables when the draws left it on none, each table's channel state one of two words or
// any; the state given a table's half the time, else either word or any.  After each the voices' span, the keys' span
// and the calls.  Not drawn: a sounding voice on no key table (rom.4 reads the word at any address, the port a table's).
// The +0x48 virtual answers a drawn word from v344 (the stops read it).  v344, the eighth part, after the static M's
// node's on the seed seven after the run's: the part's voices' stop 0x89390f70 and truncation 0x89390fd0 on a part
// node's word (a voice's +0x3c half the time, a message, now and then any) and every voice's truncation 0x89391120,
// on the listeners' draws, the virtual answering and all notes off (0x89391e30) logged.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2AssignReleaseCheck.h"
#include "Vc2CheckCalls.h"

namespace {
constexpr uint32_t kListeners = 0x89390ea0;                      // [FW]
constexpr uint32_t kListenerStates[2] = { 0x89a00002, 0x89a00042 };   // two states the tables share now and then

// v344: the virtual answering a drawn word, as the stops read it (the listeners' run leaves it unread)
void listenersInstallRom(ReleaseSides& r) {
    assignHookSlot(r.a.s, kVoicesChanged, kAssignChangeEntry, 1, assignChangedAnswer);
}

bool onATable(uint32_t channel) {
    for (uint32_t k = 0; k < (uint32_t)kReleaseTables; ++k)
        if (channel == Vc2Keys::tableAt(0, k)) return true;
    return false;
}

// The release part's draws, then every voice on a table and each table's state drawn
void steerListeners(ReleaseSides& r, std::mt19937& rng, bool all) {
    steerRelease(r, rng, all);
    for (Vc2Voice& v : r.a.s.port.voices)
        if (!onATable(v.node.channel)) v.node.channel = releaseTable(rng);
    for (uint32_t k = 0; k < (uint32_t)kReleaseTables; ++k) {
        const uint32_t picks[3] = { kListenerStates[0], kListenerStates[1], (uint32_t)rng() };
        r.keys.table(Vc2Keys::tableAt(0, k)).channel = picks[rng() % 3];
    }
}

std::string listenersEvent(ReleaseSides& r, std::mt19937& rng) {
    const uint32_t picks[4] = { r.keys.table(releaseTable(rng)).channel, kListenerStates[0], kListenerStates[1],
                                (uint32_t)rng() };
    const uint32_t state = rng() % 2 ? picks[0] : picks[1 + rng() % 3];
    ++r.n.byEvent[0];
    return releaseOnBoth(r, { kListeners, "the listeners' run" }, { state },
                         [&] { r.a.assigner.notify(state, r.keys, r.a.s.port, r.a.parts); });
}

const ReleaseEvent kListenerEvents[] = { listenersEvent };
std::span<const ReleaseEvent> listenerEvents() { return kListenerEvents; }

void listenersCount(ReleaseSides& r) {
    ++r.n.events;
    r.n.changes += vc2CountCalls(r.a.s.mine, kVoicesChanged);
}

void sayListenerCounts(const ReleaseCounts& n) {
    std::printf("listeners: %ld events, %ld change tests\n", n.events, n.changes);
}

// v344, the stops' part: the voices' and the assigner's hooks again (the voice class's part ran the queue's wait and
// the note voice's virtuals as rom.4's own), the listeners' and all notes off (0x89391e30) logged
void stopsInstallRom(ReleaseSides& r) {
    VoicesSides& s = r.a.s;
    voicesInstallRom(s.fw, s.draws, s.rom);
    assignInstallRom(r.a);
    listenersInstallRom(r);
    s.fw.cpu.hook(kAssignNotesOffAt, [&s](Sh4& c) {
        s.rom.push_back({ kVoicesOs, kAssignNotesOffAt, 0 });
        c.ret(0);
    });
}

// A part node's word: half the time a voice's +0x3c (sounding or not), else a message, any one now and then
uint32_t stopsWord(ReleaseSides& r, std::mt19937& rng) {
    const uint32_t voice = r.a.s.port.addressOf((int)(rng() % Vc2PartVoices::kVoices));
    const uint32_t picks[3] = { r.a.s.port.at(voice).part, releaseMessage(rng), (uint32_t)rng() };
    return rng() % 2 ? picks[0] : picks[1 + (rng() % 8 == 0 ? 1 : 0)];
}

// rom.4's `Routine` and the port's `Mine` on a drawn word, counted in byEvent[Counted]: the stop 0x89390f70 and the
// truncation 0x89390fd0
template <auto Mine, uint32_t Routine, size_t Counted>
std::string wordEvent(ReleaseSides& r, std::mt19937& rng) {
    const uint32_t word = stopsWord(r, rng);
    ++r.n.byEvent[Counted];
    return releaseOnBoth(r, { Routine, "a stop or truncation of a part's voices" }, { word },
                         [&] { (r.a.assigner.*Mine)(word, r.a.s.port, r.a.parts); });
}

std::string truncateAllEvent(ReleaseSides& r, std::mt19937&) {
    ++r.n.byEvent[2];
    return releaseOnBoth(r, { 0x89391120, "every voice's truncation" }, {},
                         [&] { r.a.assigner.truncateAll(r.a.s.port, r.a.parts); });
}

const ReleaseEvent kStopEvents[] = { wordEvent<&Vc2Assigner::stopPart, 0x89390f70, 0>,
                                     wordEvent<&Vc2Assigner::truncatePart, 0x89390fd0, 1>, truncateAllEvent };
std::span<const ReleaseEvent> stopEvents() { return kStopEvents; }

void stopsCount(ReleaseSides& r) {
    listenersCount(r);
    r.n.keyOffs += vc2CountCalls(r.a.s.mine, kVoicesOs);
}

void sayStopCounts(const ReleaseCounts& n) {
    std::printf("stops: %ld events: %ld stops, %ld truncations, %ld of all; %ld change tests, %ld all notes off\n",
                n.events, n.byEvent[0], n.byEvent[1], n.byEvent[2], n.changes, n.keyOffs);
}
} // namespace

long vc2AssignListenersPart(VoicesSides& s, const Vc2Run& run) {
    return voicesPart<ReleaseSides, listenersInstallRom, releaseStart, steerListeners, listenerEvents, listenersCount,
                      sayListenerCounts>(s, run, 4);
}

long vc2AssignStopsPart(VoicesSides& s, const Vc2Run& run) {
    return voicesPart<ReleaseSides, stopsInstallRom, releaseStart, steerListeners, stopEvents, stopsCount,
                      sayStopCounts>(s, run, 7);
}
#endif
