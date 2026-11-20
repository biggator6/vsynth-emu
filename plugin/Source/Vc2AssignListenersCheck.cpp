// xt_vc2voices' fifth part (v305): the listeners' run, Vc2Assigner::notify, against rom.4's own 0x89390ea0 on its own
// voices and keys, after the voice class's part on the seed after its.  The release part's sides, start and draws, and
// beyond them the +0x48 virtual of both classes (the note's change test 0x893972d0, which the fourth part ran as
// rom.4's own, and the plain voice's 0x89391720) hooked to log the voice and its +0x3c.  Before each event every voice
// put on one of the three key tables when the draws left it on none, each table's channel state one of two words or
// any; the state given a table's half the time, else either word or any.  After each the voices' span, the keys' span
// and the calls.  Not drawn: a sounding voice on no key table (rom.4 reads the word at any address, the port a table's).
// XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2AssignReleaseCheck.h"
#include "Vc2CheckCalls.h"

namespace {
constexpr uint32_t kListeners = 0x89390ea0;                      // [FW]
constexpr uint32_t kListenerStates[2] = { 0x89a00002, 0x89a00042 };   // two states the tables share now and then

void listenersInstallRom(ReleaseSides& r) { assignHookSlot(r.a.s, kVoicesChanged, kAssignChangeEntry, 1); }

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
} // namespace

long vc2AssignListenersPart(VoicesSides& s, const Vc2Run& run) {
    return voicesPart<ReleaseSides, listenersInstallRom, releaseStart, steerListeners, listenerEvents, listenersCount,
                      sayListenerCounts>(s, run, 4);
}
#endif
