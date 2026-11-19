// xt_vc2voices <rom.4> [cases] [seed] (v272): the VC-2 part's voices and their lists, Vc2PartVoices, against rom.4's
// own routines on its own voices, heads and words in .bss.  What they call beyond themselves hooked on rom.4's side and
// faked on the port's (Vc2PartVoicesFakes.cpp): the two voices' vtables' destroys, the tone's end test its vtable
// reaches and the tone's cost, each logged with its routine and object and answered from one queue drawn for both
// sides; a destroy writes the voice's note, state, +0x30, +0x44 and age as drawn, so the moves' order around it shows.
// The notes W and the tones the voices read are rom.4's own, their words drawn in its memory.  Each case starts from
// noise and rom.4's three inits in its order, then 400 events (Vc2PartVoicesEvents.cpp): the take, the steal's two
// parts, the three moves, the reserve, the ended scan, stop all, the costs, the budget's setters and getters, an init
// again.  Before each, voices' words drawn now and then (vtable, +0x44, state, age and its pointer, note), the notes'
// kinds and tones, the offset and the counts, and the lists: the prepared list moved onto the sounding list's front as
// the assigner's start does (0x89390860), or every voice dealt to a list anew; a sounding voice always has a note.
// After each, the whole span 0x89981b6c…0x89982710 word for word (the port's words, and nothing else moved), the calls
// and the answer.  XT_VC2VOICES_FIRST=1 says where the first case to differ does, XT_VC2VOICES_STOP=1 stops there.
// XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2KeysTables.h"
#include "Vc2PartVoicesCheck.h"
#include "Vc2PartVoicesTables.h"
#include <algorithm>
#include <bit>
#include <numeric>

namespace {
using V = Vc2PartVoices;
constexpr int kSteps = 400;
constexpr uint32_t kSpanEnd = V::kAgesAt + 4;
constexpr uint32_t kOrderAt = 0x20, kOrderBytes = 4;   // [FW] a voice's two ages

uint32_t spanFrom() { return std::ranges::min(vc2PartVoicesTables().voices); }
size_t spanBytes() { return kSpanEnd - spanFrom(); }

// The port's words laid over a copy of the span
void overlay(std::vector<uint32_t>& span, const V& port) {
    const uint32_t from = spanFrom();
    const auto put = [&](uint32_t at, const void* words, size_t bytes) {
        std::memcpy(span.data() + (at - from) / 4, words, bytes);
    };
    for (int i = 0; i < V::kVoices; ++i) put(port.addressOf(i), &port.voices[(size_t)i], sizeof(Vc2Voice));
    put(V::kHeadsAt, port.heads.data(), sizeof port.heads);
    put(V::kGlobalsAt, &port.g, sizeof port.g);
    put(V::kAgesAt, &port.ages, sizeof port.ages);
}

// A word of the span by what holds it
std::string spanWordName(const V& port, uint32_t at) {
    char t[96];
    for (int i = 0; i < V::kVoices; ++i) {
        if (at - port.addressOf(i) >= sizeof(Vc2Voice)) continue;
        std::snprintf(t, sizeof t, "voice %d's +0x%02x", i, at - port.addressOf(i));
        return t;
    }
    if (at - V::kHeadsAt < sizeof port.heads)
        std::snprintf(t, sizeof t, "list %u's head's +0x%02x", (at - V::kHeadsAt) / V::kHeadBytes,
                      (at - V::kHeadsAt) % V::kHeadBytes);
    else
        std::snprintf(t, sizeof t, "the word 0x%08x", at);
    return t;
}

uint32_t drawNote(std::mt19937& rng) { return kVoicesNotesAt + kVoicesNoteBytes * (uint32_t)(rng() % kVoicesNotes); }

// An age: few apart, so the take's ties and order show; a prepare's −1, a note's count, 0, or any
uint32_t drawOrder(std::mt19937& rng) {
    const uint32_t picks[5] = { rng() % 8, 0xffffffffu, 0x70000000u | (uint32_t)(rng() % 32), 0, (uint32_t)rng() };
    return picks[rng() % 5];
}

// A voice's words the routines decide by
void drawVoiceWords(Vc2VoiceNode& v, uint32_t self, std::mt19937& rng) {
    v.vtable = rng() % 2 ? V::kVoiceVtable : kVoicesNoteClass;
    v.leave = rng() % 3 ? V::kDestroyOnLeave : V::kStayOnLeave;
    v.state = rng() % 6 ? (uint8_t)(rng() % 4) : (uint8_t)rng();
    const uint32_t age = rng() % 3;
    v.age = age == 0 ? V::kZeroAge : self + kOrderAt + kOrderBytes * (age - 1);
    for (uint32_t& order : v.order) order = drawOrder(rng);
    v.note = rng() % 4 ? drawNote(rng) : 0;
}

// The notes' kinds and tones in rom.4's memory, and each tone's vtable
void drawNotes(Vc2Fw& fw, std::mt19937& rng) {
    constexpr uint8_t kKinds[7] = { 0, 0, 0, 1, 1, 2, 0xff };
    for (uint32_t k = 0; k < kVoicesNotes; ++k) {
        const uint32_t note = kVoicesNotesAt + kVoicesNoteBytes * k;
        fw.cpu.wr32(note + kVoicesNoteTone, kVoicesTonesAt + kVoicesToneBytes * (uint32_t)(rng() % kVoicesTones));
        const uint8_t kind = rng() % 8 ? kKinds[rng() % 7] : (uint8_t)rng();
        fw.cpu.write(note + kVoicesNoteKind, &kind, 1);
    }
    for (uint32_t t = 0; t < kVoicesTones; ++t)
        fw.cpu.wr32(kVoicesTonesAt + kVoicesToneBytes * t + kVoicesToneVtable, kVoicesToneClass);
}

// The words beside the lists: the reserve's count, the budget, the offset, an unread float, the next note's age
void steerGlobals(V& port, std::mt19937& rng) {
    port.g.reserved = (int32_t)(rng() % 25);
    port.g.budget = std::bit_cast<float>((uint32_t)rng());
    const float offsets[4] = { 0.0f, 1e-5f, 6e-5f, 1e-30f };
    port.g.offset = rng() % 4 ? offsets[rng() % 4] : 2.0f * vc2Unit(rng) - 1.0f;
    port.g.unread0 = std::bit_cast<float>((uint32_t)rng());
    port.ages = rng() % 2 ? (uint32_t)rng() : rng() % 64;
}

// `voices` as `head`'s list, in order
void chainVoices(V& port, uint32_t head, const std::vector<uint32_t>& voices) {
    uint32_t last = head;
    for (const uint32_t v : voices) {
        port.at(last).next = v;
        port.at(v).prev = last;
        last = v;
    }
    port.at(last).next = head;
    port.at(head).prev = last;
}

// Every voice dealt to a list anew, in a shuffled order; one time in four none to the free list
void dealVoices(VoicesSides& s, std::mt19937& rng) {
    constexpr int kDeal[11] = { V::kSounding, V::kSounding, V::kSounding, V::kSounding, V::kFree, V::kFree, V::kFree,
                                V::kPrepared, V::kPrepared, V::kReserved, V::kReserved };
    const bool noFree = rng() % 4 == 0;
    std::array<int, V::kVoices> order {};
    std::iota(order.begin(), order.end(), 0);
    std::ranges::shuffle(order, rng);
    std::array<std::vector<uint32_t>, V::kLists> lists {};
    for (const int i : order) {
        const int dealt = kDeal[rng() % 11];
        lists[(size_t)(noFree && dealt == V::kFree ? V::kSounding : dealt)].push_back(s.port.addressOf(i));
    }
    for (size_t k = 0; k < lists.size(); ++k) chainVoices(s.port, s.port.g.lists[k], lists[k]);
    ++s.n.redraws;
}

// The prepared list onto the sounding list's front, its voices given notes first, as the assigner's start does
void spliceVoices(VoicesSides& s, std::mt19937& rng) {
    const uint32_t sounding = s.port.g.lists[V::kSounding], prepared = s.port.g.lists[V::kPrepared];
    voicesGiveNotes(s, prepared, rng);
    std::vector<uint32_t> voices = voicesOf(s, prepared);
    std::ranges::copy(voicesOf(s, sounding), std::back_inserter(voices));
    chainVoices(s.port, sounding, voices);
    chainVoices(s.port, prepared, {});
    ++s.n.splices;
}

// The draws before an event, on the port's words (all of them at a case's first)
void steerVoices(VoicesSides& s, std::mt19937& rng, bool all) {
    for (int i = 0; i < V::kVoices; ++i)
        if (all || rng() % 8 == 0) drawVoiceWords(s.port.at(s.port.addressOf(i)), s.port.addressOf(i), rng);
    if (all || rng() % 16 == 0) steerGlobals(s.port, rng);
    if (rng() % 20 == 0) drawNotes(s.fw, rng);
    if (rng() % 30 == 0) dealVoices(s, rng);
    if (rng() % 3 == 0) spliceVoices(s, rng);
    voicesGiveNotes(s, s.port.g.lists[V::kSounding], rng);
}

// A case of the check: the port's words noise, rom.4's three inits, then the events
std::string voicesCase(VoicesSides& s, std::mt19937& rng) {
    std::vector<uint32_t> noise(sizeof s.port / 4);
    for (uint32_t& w : noise) w = (uint32_t)rng();
    std::memcpy(&s.port, noise.data(), sizeof s.port);
    drawNotes(s.fw, rng);
    std::string what = voicesInits(s);
    const std::span<const VoicesEvent> events = voicesEvents();
    for (int k = 0; k < kSteps && what.empty(); ++k, ++s.n.events) {
        steerVoices(s, rng, k == 0);
        voicesPrepare(s, (uint32_t)rng());
        what = events[rng() % events.size()](s, rng);
        s.n.destroys += vc2CountCalls(s.mine, kVoicesDestroy);
        s.n.ended += vc2CountCalls(s.mine, kVoicesEnded);
    }
    return what;
}

void sayVoiceCounts(const Vc2Run& run, const VoicesCounts& n) {
    std::printf("voices: %d cases, %ld events, %ld inits, %ld takes (%ld from free, %ld stolen, %ld none), %ld "
                "moves, %ld reserves, %ld ended scans (%ld end tests), %ld stop alls, %ld stops, %ld costs, %ld "
                "budgets; %ld destroys; %ld splices, %ld lists dealt\n", run.cases, n.events, n.inits, n.takes,
                n.fromFree, n.stolen, n.none, n.moves, n.reserves, n.scans, n.ended, n.stopAlls, n.stops, n.costs,
                n.getters, n.destroys, n.splices, n.redraws);
}
} // namespace

void voicesSteer(VoicesSides& s, std::mt19937& rng, bool all) { steerVoices(s, rng, all); }
void voicesDrawNotes(Vc2Fw& fw, std::mt19937& rng) { drawNotes(fw, rng); }

std::vector<uint32_t> voicesOf(VoicesSides& s, uint32_t list) {
    std::vector<uint32_t> voices;
    for (uint32_t v = s.port.at(list).next; v != list && voices.size() <= V::kVoices; v = s.port.at(v).next)
        voices.push_back(v);
    return voices;
}

void voicesGiveNotes(VoicesSides& s, uint32_t list, std::mt19937& rng) {
    for (const uint32_t v : voicesOf(s, list))
        if (s.port.at(v).note == 0) s.port.at(v).note = drawNote(rng);
}

void voicesPrepare(VoicesSides& s, uint32_t seed) {
    s.around.resize(spanBytes() / 4);
    s.fw.cpu.read(spanFrom(), s.around.data(), spanBytes());
    overlay(s.around, s.port);
    s.fw.cpu.write(spanFrom(), s.around.data(), spanBytes());
    s.rom.clear();
    s.mine.clear();
    s.draws.restart(seed);
}

std::string voicesDifference(VoicesSides& s) {
    std::vector<uint32_t> ours = s.around, theirs(s.around.size());
    overlay(ours, s.port);
    s.fw.cpu.read(spanFrom(), theirs.data(), spanBytes());
    const auto [a, b] = std::ranges::mismatch(theirs, ours);
    if (a == theirs.end()) return voicesCallsDifference(s.rom, s.mine);
    const uint32_t at = spanFrom() + 4 * (uint32_t)(a - theirs.begin());
    char t[160];
    std::snprintf(t, sizeof t, "%s (0x%08x): %08x, against %08x", spanWordName(s.port, at).c_str(), at, *b, *a);
    return t;
}

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2voices", vc2LoadAll<vc2LoadPartVoices, vc2LoadKeys>);
    if (!run) return 1;
    Vc2PartVoices port;
    VoicesDraws draws;
    std::vector<VoicesCall> rom, mine;
    VoicesPortParts parts(fw, port, draws, mine);
    VoicesSides s { fw, port, parts, draws, rom, mine };
    voicesInstallRom(fw, draws, rom);
    Vc2Tally tally { "XT_VC2VOICES_FIRST", "XT_VC2VOICES_STOP" };
    vc2RunCases(*run, tally, "", s.n.events, [&](std::mt19937& rng, bool) { return voicesCase(s, rng); });
    sayVoiceCounts(*run, s.n);
    const long assigner = vc2AssignPart(s, *run) + vc2AssignReleasePart(s, *run) + vc2VoiceClassPart(s, *run);
    std::printf("xt_vc2voices: %ld different\n", tally.different + assigner);
    return 0;
}
#endif
