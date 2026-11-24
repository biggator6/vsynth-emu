// xt_vc2synth's notes part (v346): the part's MIDI note path against rom.4's own, after the load part and before the
// reads part, on the seed three after the run's.  [cases] cases (16 at the least), each a patch loaded on both sides
// (a store of noise or none, the part's read, the system's dispatchers and part 0's message, its node the held class),
// then 40 MIDI messages, each as rom.4 takes it: the OS MIDI's handler 0x89314e90 then the notes' task's dispatcher
// 0x8938fed0, each given (0, the message, its word), on rom.4's side; `Vc2Synth::midiMessage` on the part's.  A
// message: a note-on (two in five: a key of a few near the middle, a velocity 1…127, now and then 0), a note-off (a key
// one of the same few), a controller (one the part has a class for, or any number below 128; a value below 128), a
// bend, either pressure or a program change; on source 1 mostly (2, 3 now and then), the channel part 0 receives on
// mostly (any now and then); or one in six a parameters' message (0x40, 0x41: its word a kind 0…13, 3 with Out DC cut
// one time in four, and a second byte) or the reset (0x70), the OS's routines these run on rom.4's side
// (Vc2OsMidiParts::kOsRoutines) hooked as the part leaves them.  After each every region of the part, the vocoder, the
// flags posted the notes' task and each tone's words.  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2SynthCheck.h"
#include "Vc2SynthMidi.h"

namespace {
constexpr uint32_t kHandler = 0x89314e90, kDispatcher = 0x8938fed0;   // [FW]
constexpr uint32_t kMessageAt = 0x8C170000, kWordAt = 0x8C170010;      // in Vc2Fw's RAM: a message, its word
constexpr int kEvents = 40, kKeys = 6, kWordKinds = 14;
// The controllers drawn mostly: those the part has a class for (Vc2PartNumbers' rows) and the modulation wheel
constexpr uint8_t kControllers[] = { 1, 5, 6, 7, 10, 11, 38, 64, 65, 66, 68, 71, 72, 73, 74, 84, 91, 93,
                                     120, 121, 123, 124, 125, 126, 127 };
// A message's kind by a draw of 24: the note-on, note-off, controller, bend, poly pressure, pressure, program change,
// the parameters' message, the part's, the reset
constexpr std::array<uint8_t, 24> kKindByDraw { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 3, 4, 5, 6, 7, 7, 8,
                                                9 };
constexpr uint8_t kClasses[] = { 0x90, 0x80, 0xb0, 0xe0, 0xa0, 0xd0, 0xc0, 0x40, 0x41, 0x70 };   // [FW] by kind
constexpr size_t kWordFrom = 7, kResetKind = 9;   // the kinds given a word, the reset's

struct NotesCounts {
    long events = 0, voiced = 0;   // voiced: the messages after which a note's age is new (a voice started)
    std::array<long, std::size(kClasses)> byKind {};
};

// A message drawn, and its word: its status's class by kind, its data, its source; the parameters' messages a word of
// a kind 0…13 (3, the system's, with Out DC cut, one time in four) and its second byte (0 half the time)
struct Drawn {
    Vc2MidiMessage m;
    uint32_t word = 0;
};
Drawn messageDrawn(std::mt19937& rng, NotesCounts& n, uint8_t receiving) {
    const size_t kind = kKindByDraw[rng() % kKindByDraw.size()];
    ++n.byKind[kind];
    const uint8_t channel = rng() % 8 == 0 || kind >= kWordFrom ? (uint8_t)(rng() % 16) : receiving;
    const uint8_t key = (uint8_t)(57 + 3 * (rng() % kKeys));
    const uint8_t number = rng() % 4 ? kControllers[rng() % std::size(kControllers)] : (uint8_t)(rng() % 128);
    Drawn d;
    d.m.status = (uint8_t)(kClasses[kind] | (kind < kWordFrom ? channel : 0));
    d.m.data1 = kind <= 1 ? key : kind == 2 ? number : (uint8_t)(rng() % 128);
    d.m.data2 = (uint8_t)(rng() % 128);
    if (kind == 0 && d.m.data2 == 0 && rng() % 4) d.m.data2 = (uint8_t)(1 + rng() % 127);
    d.m.source = rng() % 8 == 0 ? (uint8_t)(2 + rng() % 2) : 1;
    const uint32_t wordKind = rng() % 4 == 0 ? 3 : rng() % kWordKinds, second = rng() % 2 ? 0 : rng() % 256;
    if (kind >= kWordFrom && kind != kResetKind) d.word = wordKind | second << 8;
    return d;
}

// The channel part 0's node of source 1 receives on (0 when none, or every one): where the notes go mostly
uint8_t receivingOf(const Vc2Synth& s) {
    const int8_t c = s.nodes.of(1, 0).channel;
    return c < 0 ? 0 : (uint8_t)c;
}

// One message on both sides, then what differs: every region, the vocoder, the events, each tone's words
std::string partMidiEvent(Vc2Fw& fw, Vc2Synth& s, const Drawn& m, const Vc2SynthPosts& posts) {
    fw.cpu.write(kMessageAt, &m.m, sizeof m.m);
    fw.cpu.wr32(kWordAt, m.word);
    if (!fw.call(kHandler, { 0, kMessageAt, kWordAt })) return "the OS MIDI's handler faults: " + fw.cpu.error();
    if (!fw.call(kDispatcher, { 0, kMessageAt, kWordAt })) return "the dispatcher faults: " + fw.cpu.error();
    s.midiMessage(m.m, m.word);
    std::string d = vc2SynthStepDifference(fw, s, posts);
    for (int tone = 0; tone < Vc2TonePool::kTones && d.empty(); ++tone) d = vc2SynthToneDifference(fw, s, tone);
    return d;
}
} // namespace

long vc2SynthNotesPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, const Vc2SynthPosts& posts) {
    NotesCounts n;
    const auto oneCase = [&](std::mt19937& rng, std::vector<uint8_t>& store) {
        std::string d = vc2SynthPatchLoaded(fw, s, store, rng, posts);
        for (int e = 0; e < kEvents && d.empty(); ++e) {
            ++n.events;
            const uint32_t age = s.voices.ages;
            d = partMidiEvent(fw, s, messageDrawn(rng, n, receivingOf(s)), posts);
            n.voiced += s.voices.ages != age && s.voices.ages != 0 ? 1 : 0;
        }
        return d;
    };
    const long different = vc2SynthCases(fw, s, run, 3, "the notes", oneCase);
    std::printf("synth notes: %d cases, %ld events: %ld note-ons, %ld note-offs, %ld controllers, %ld bends, %ld poly "
                "pressures, %ld pressures, %ld programs, %ld parameters', %ld the part's, %ld resets; %ld voiced\n",
                vc2SynthCaseCount(run), n.events, n.byKind[0], n.byKind[1], n.byKind[2], n.byKind[3], n.byKind[4],
                n.byKind[5], n.byKind[6], n.byKind[7], n.byKind[8], n.byKind[9], n.voiced);
    return different;
}
#endif
