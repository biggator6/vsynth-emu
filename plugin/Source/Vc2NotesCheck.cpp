// xt_vc2notes <rom.4> [cases] [seed] (v275): the VC-2 part's notes W, Vc2Notes, against rom.4's own routines on its own
// notes and hook word in .bss.  Every tone method they reach (the make, the end test through the tone's own vtable, the
// unmake, both truncations, the note-on, the lists' start, the seven block setters, the key-off and the position) and
// the hook word's two routines hooked on rom.4's side and faked on the port's (Vc2NotesFakes.cpp), each logged with the
// argument registers it reads and answered alike on both sides; the tones are the pool's, their vtable, serial and the
// two routines a constructor gives them in rom.4's memory.  Each case starts from noise, every note given a tone, then
// 400 events (Vc2NotesEvents.cpp): the constructor, the note-on with a partner or none, the end of a note or none, the
// start and its copy, the two block passes, the release, the truncation once, the position, the placement, the two
// routines a tone is given and the hook's initializer.  Before each, a note's tone, bytes and words drawn now and
// then, the hook word, and the tones' serials.  After each, the hook word and the 24 notes word for word, the tones'
// two routines, the calls and the answer.  XT_VC2NOTES_FIRST=1 says where the first case to differ does,
// XT_VC2NOTES_STOP=1 stops there.  XT_VC2NOTES_MAIN only.
#ifdef XT_VC2NOTES_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2NotesCheck.h"
#include <cstring>
#include <memory>

namespace {
using N = Vc2Notes;
constexpr int kSteps = 400;
constexpr uint32_t kNoteBytes = sizeof(Vc2NoteWords);
constexpr size_t kHeadWords = 2;   // the hook word and the unread float, before the notes
constexpr size_t kSpanWords = kHeadWords + sizeof(Vc2NoteWords) * N::kNotes / 4;

// The port's words as rom.4 lays them out from the hook word
std::vector<uint32_t> notesSpan(const N& port) {
    std::vector<uint32_t> span(kSpanWords);
    span[0] = port.hook;
    span[1] = port.unread;
    std::memcpy(span.data() + kHeadWords, port.notes.data(), sizeof port.notes);
    return span;
}

std::string notesWordName(uint32_t at) {
    char t[64];
    const uint32_t from = at - N::kNotesAt;
    if (at < N::kNotesAt)
        std::snprintf(t, sizeof t, "the word 0x%08x", at);
    else
        std::snprintf(t, sizeof t, "note %u's +0x%03x", from / kNoteBytes, from % kNoteBytes);
    return t;
}

uint32_t drawHook(std::mt19937& rng) { return rng() % 2 ? N::kNoHook : kNotesTaskHook; }

// A note's own words beyond its input: its tone always the pool's, the four bytes as the routines meet them
void drawNoteTail(Vc2NoteWords& w, std::mt19937& rng) {
    w.tone = noteToneAt((int)(rng() % kNotesTones));
    w.partner = rng() % 2 ? 0 : N::kNotesAt + kNoteBytes * (uint32_t)(rng() % N::kNotes);
    w.onByte = (uint8_t)rng();
    w.part = (uint8_t)rng();
    w.kind = (uint8_t)(rng() % 3);
    const uint8_t truncated[4] = { 0, 0, 1, (uint8_t)rng() };
    w.truncated = truncated[rng() % 4];
}

// The tones' serials in rom.4's memory, which a partner's note-on reads
void drawSerials(Vc2Fw& fw, std::mt19937& rng) {
    for (int k = 0; k < kNotesTones; ++k)
        fw.cpu.wr32(noteToneAt(k) + kNotesToneSerial, rng() % 4 ? rng() % 64 : (uint32_t)rng());
}

// The draws before an event
void steerNotes(NoteSides& s, std::mt19937& rng) {
    if (rng() % 3 == 0) drawNoteTail(s.port.notes[rng() % N::kNotes], rng);
    if (rng() % 4 == 0) s.port.notes[rng() % N::kNotes].input.words[rng() % (Vc2ToneInput::kBytes / 4)] = rng();
    if (rng() % 16 == 0) s.port.hook = drawHook(rng);
    if (rng() % 20 == 0) drawSerials(s.fw, rng);
}

// A case of the check: the port's words noise, each note given a tone of the pool's, the tones' words drawn, then the
// events
std::string notesCase(NoteSides& s, std::mt19937& rng) {
    std::vector<uint32_t> noise(kSpanWords);
    for (uint32_t& w : noise) w = (uint32_t)rng();
    s.port.unread = noise[1];
    std::memcpy(s.port.notes.data(), noise.data() + kHeadWords, sizeof s.port.notes);
    for (Vc2NoteWords& w : s.port.notes) drawNoteTail(w, rng);
    s.port.hook = drawHook(rng);
    for (auto& routines : s.parts.routines)
        for (uint32_t& r : routines) r = (uint32_t)rng();
    for (int k = 0; k < kNotesTones; ++k) s.fw.cpu.wr32(noteToneAt(k) + kNotesToneVtable, kNotesToneClass);
    drawSerials(s.fw, rng);
    std::string what;
    const std::span<const NoteEvent> events = noteEvents();
    for (int k = 0; k < kSteps && what.empty(); ++k, ++s.n.events) {
        steerNotes(s, rng);
        notesPrepare(s, (uint32_t)rng());
        what = events[rng() % events.size()](s, rng);
        for (int32_t kind = 0; kind < kNoteKinds; ++kind) s.n.calls[(size_t)kind] += vc2CountCalls(s.mine, kind);
    }
    return what;
}

void sayNoteCounts(const Vc2Run& run, const NoteCounts& n, long different) {
    const auto& e = n.byEvent;
    const auto& c = n.calls;
    std::printf("notes: %d cases, %ld events: %ld constructors, %ld note-ons (%ld with a partner), %ld ends, %ld and "
                "%ld passes, %ld releases, %ld truncations once, %ld starts and %ld copies', %ld placements, %ld "
                "positions, %ld and %ld of the tone's routines, %ld initializers\n", run.cases, n.events, e[7], e[8],
                n.partners, e[0], e[1], e[2], e[3], e[4], e[9], e[10], e[11], e[12], e[5], e[6], e[13]);
    std::printf("their calls: %ld makes, %ld end tests, %ld unmakes, %ld truncations (%ld at once), %ld tone note-ons, "
                "%ld starts, %ld block setters, %ld key-offs, %ld positions, %ld hooks\n", c[kNoteMake],
                c[kNoteEndTest], c[kNoteUnmake], c[kNoteTruncateWait] + c[kNoteTruncateNow], c[kNoteTruncateNow],
                c[kNoteOnTone], c[kNoteStart], c[kNoteSet], c[kNoteKeyOff], c[kNotePosition], c[kNoteHook]);
    std::printf("xt_vc2notes: %ld different\n", different);
}
} // namespace

NoteSides::NoteSides(Vc2Fw& rig) : fw(rig), parts(rig, port, answers, mine) { notesInstallRom(rig, answers, rom); }

void notesPrepare(NoteSides& s, uint32_t seed) {
    const std::vector<uint32_t> span = notesSpan(s.port);
    s.fw.cpu.write(N::kHookAt, span.data(), span.size() * 4);
    for (int k = 0; k < kNotesTones; ++k)
        s.fw.cpu.write(noteToneAt(k) + kNotesToneRoutines, s.parts.routines[(size_t)k].data(), 8);
    s.rom.clear();
    s.mine.clear();
    s.answers.restart(seed);
}

std::string notesDifference(NoteSides& s) {
    const std::vector<uint32_t> ours = notesSpan(s.port);
    std::vector<uint32_t> theirs(ours.size());
    s.fw.cpu.read(N::kHookAt, theirs.data(), theirs.size() * 4);
    char t[160];
    const auto [a, b] = std::ranges::mismatch(theirs, ours);
    if (a != theirs.end()) {
        const uint32_t at = N::kHookAt + 4 * (uint32_t)(a - theirs.begin());
        std::snprintf(t, sizeof t, "%s (0x%08x): %08x, against %08x", notesWordName(at).c_str(), at, *b, *a);
        return t;
    }
    for (int k = 0; k < kNotesTones; ++k) {
        const std::array<uint32_t, 2>& mine = s.parts.routines[(size_t)k];
        std::array<uint32_t, 2> rom {};
        s.fw.cpu.read(noteToneAt(k) + kNotesToneRoutines, rom.data(), 8);
        if (rom == mine) continue;
        std::snprintf(t, sizeof t, "tone %d's routines: %08x %08x, against %08x %08x", k, mine[0], mine[1], rom[0],
                      rom[1]);
        return t;
    }
    return notesCallsDifference(s.rom, s.mine);
}

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2notes", vc2LoadAll<>);
    if (!run) return 1;
    const auto s = std::make_unique<NoteSides>(fw);
    Vc2Tally tally { "XT_VC2NOTES_FIRST", "XT_VC2NOTES_STOP" };
    vc2RunCases(*run, tally, "", s->n.events, [&](std::mt19937& rng, bool) { return notesCase(*s, rng); });
    sayNoteCounts(*run, s->n, tally.different);
    return 0;
}
#endif
