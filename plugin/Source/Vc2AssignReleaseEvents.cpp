// xt_vc2voices' third part's events (v278): each of the release path's routines and the part node's note start and
// off on both sides, rom.4's then the port's; what differs after, the voices' span, the calls and the keys' span.
// XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2AssignReleaseCheck.h"

namespace {
using V = Vc2PartVoices;
using OnTable = void (Vc2Assigner::*)(uint32_t, Vc2Keys&, Vc2PartVoices&, Vc2AssignParts&);
using OnVoice = void (Vc2Assigner::*)(uint32_t, Vc2PartVoices&, Vc2AssignParts&);

uint32_t releaseVoice(ReleaseSides& r, std::mt19937& rng) { return r.a.s.port.addressOf((int)(rng() % V::kVoices)); }

std::string releaseKeyEvent(ReleaseSides& r, std::mt19937& rng) {
    const uint32_t voice = releaseVoice(r, rng), table = releaseTable(rng);
    ++r.n.byEvent[0];
    return releaseOnBoth(r, { 0x89391760, "a key's release" }, { voice, table },
                         [&] { r.a.assigner.release(voice, r.keys.table(table), r.a.s.port, r.a.parts); });
}

// [FW] the pedals' lifts on a voice
const std::pair<Vc2CheckRoutine, OnVoice> kOnVoice[] = {
    { { 0x89391820, "the Hold's lift" }, &Vc2Assigner::liftHold },
    { { 0x89391880, "the Sostenuto's lift" }, &Vc2Assigner::liftSostenuto },
};

template <size_t I>
std::string releaseVoiceEvent(ReleaseSides& r, std::mt19937& rng) {
    const uint32_t voice = releaseVoice(r, rng);
    ++r.n.byEvent[1 + I];
    return releaseOnBoth(r, kOnVoice[I].first, { voice },
                         [&] { (r.a.assigner.*kOnVoice[I].second)(voice, r.a.s.port, r.a.parts); });
}

// [FW] the routines over a key table's voices
const std::pair<Vc2CheckRoutine, OnTable> kOnTable[] = {
    { { 0x89390c00, "the note-off" }, &Vc2Assigner::noteOff },
    { { 0x89390cd0, "the Hold off" }, &Vc2Assigner::holdOff },
    { { 0x89390d10, "the Sostenuto off" }, &Vc2Assigner::sostenutoOff },
    { { 0x89390da0, "the release but kind 1" }, &Vc2Assigner::releaseBut },
    { { 0x89390e20, "the release of all" }, &Vc2Assigner::releaseAll },
};
constexpr size_t kOnTableEvent[5] = { 3, 4, 5, 7, 8 };

template <size_t I>
std::string releaseTableEvent(ReleaseSides& r, std::mt19937& rng) {
    const uint32_t table = releaseTable(rng);
    ++r.n.byEvent[kOnTableEvent[I]];
    return releaseOnBoth(r, kOnTable[I].first, { table },
                         [&] { (r.a.assigner.*kOnTable[I].second)(table, r.keys, r.a.s.port, r.a.parts); });
}

std::string releaseTruncateEvent(ReleaseSides& r, std::mt19937& rng) {
    const uint32_t table = releaseTable(rng);
    ++r.n.byEvent[6];
    return releaseOnBoth(r, { 0x89390d50, "the truncation of a table" }, { table },
                         [&] { r.a.assigner.truncateTable(table, r.a.s.port, r.a.parts); });
}

// A node's note: its node, table and message, a value from −12 to 12 or any, a word passed on the stack
Vc2NodeNote releaseNote(std::mt19937& rng) {
    Vc2NodeNote n;
    n.node = releaseNode(rng);
    n.table = releaseTable(rng);
    n.message = releaseMessage(rng);
    n.value = rng() % 4 ? (int32_t)(rng() % 25) - 12 : (int32_t)rng();
    n.passed = (uint32_t)rng();
    return n;
}

std::string releaseNoteOnEvent(ReleaseSides& r, std::mt19937& rng) {
    const Vc2NodeNote n = releaseNote(rng);
    ++r.n.byEvent[9];
    r.a.s.fw.cpu.wr32(kVc2Sp, n.passed);
    return releaseOnBoth(r, { 0x89396a80, "the node's note start" }, { n.node, n.table, n.message, (uint32_t)n.value },
                         [&] { r.a.assigner.nodeNoteOn(n, r.keys, r.a.s.port, r.a.parts); });
}

std::string releaseNoteOffEvent(ReleaseSides& r, std::mt19937& rng) {
    const Vc2NodeNote n = releaseNote(rng);
    ++r.n.byEvent[10];
    // Half the time the node's first glide sounding the table's note-off key, so a key held is found
    const uint32_t first = Vc2Keys::kGlidesAt + 0x480 * r.keys.table(n.table).source + 0x48 * r.a.s.fw.cpu.rd8(n.node);
    Vc2KeyGlide& g = r.keys.glide(first);
    if (rng() % 2) g.key = r.keys.table(g.table).offKey;
    return releaseOnBoth(r, { 0x89396c90, "the node's note-off" }, { n.node, n.table, n.message },
                         [&] { r.a.assigner.nodeNoteOff(n, r.keys, r.a.s.port, r.a.parts); });
}

const ReleaseEvent kEvents[] = {
    releaseKeyEvent,      releaseKeyEvent,      releaseVoiceEvent<0>, releaseVoiceEvent<1>, releaseTableEvent<0>,
    releaseTableEvent<0>, releaseTableEvent<1>, releaseTableEvent<2>, releaseTruncateEvent, releaseTableEvent<3>,
    releaseTableEvent<4>, releaseNoteOnEvent,   releaseNoteOnEvent,   releaseNoteOnEvent,   releaseNoteOffEvent,
    releaseNoteOffEvent,
};
} // namespace

std::span<const ReleaseEvent> releaseEvents() { return kEvents; }
#endif
