// xt_vc2notes' events (v275): each of the notes' routines on both sides, rom.4's then the port's, and what differs
// after: the notes' words, the tones' routines, the calls and the answer.  XT_VC2NOTES_MAIN only.
#ifdef XT_VC2NOTES_MAIN
#include "Vc2NotesCheck.h"

namespace {
using N = Vc2Notes;
constexpr uint32_t kNoteBytes = sizeof(Vc2NoteWords);   // [FW] the placement's first argument, which it doesn't read

// rom.4's `routine` on `args`, then the port's `mine()` giving its answer; then what differs, and rom.4's r0 against
// that answer when the routine `answers`
template <class Mine>
std::string notesOnBoth(NoteSides& s, uint32_t routine, std::initializer_list<uint32_t> args, const char* name,
                        bool answers, Mine mine) {
    uint32_t ours = 0;
    return vc2BothOn(s.fw, routine, args, name, [&] { ours = mine(); }, [&] {
        const std::string what = notesDifference(s);
        if (!what.empty() || !answers || s.fw.cpu.r[0] == ours) return what;
        char t[96];
        std::snprintf(t, sizeof t, "%s answers %08x, against %08x", name, ours, s.fw.cpu.r[0]);
        return std::string(t);
    });
}

uint32_t notesAny(NoteSides& s, std::mt19937& rng) { return s.port.addressOf(rng() % N::kNotes); }
uint32_t notesOrNone(NoteSides& s, std::mt19937& rng) { return rng() % 6 == 0 ? 0 : notesAny(s, rng); }
uint32_t notesTone(NoteSides&, std::mt19937& rng) { return noteToneAt((int)(rng() % kNotesTones)); }

// A word as the callers give it: a byte's or a number's, or any
uint32_t notesWord(std::mt19937& rng) {
    const uint32_t picks[4] = { rng() % 2, rng() % 256, (uint32_t)rng(), rng() % 24 };
    return picks[rng() % 4];
}

// A routine of one argument on both sides: rom.4's at its address, the port's member
struct NotesOneArg {
    const char* name;
    uint32_t routine;
    void (N::*mine)(uint32_t, Vc2NoteParts&);
    uint32_t (*draw)(NoteSides&, std::mt19937&);
};

// [FW] the routines of a note (or of none), and of a tone
constexpr NotesOneArg kOneArg[] = {
    { "the end", 0x89396590, &N::end, notesOrNone },
    { "the settings pass", 0x89396650, &N::setBlocks, notesAny },
    { "the update pass", 0x893967a0, &N::setBlocks, notesAny },
    { "the release", 0x893968b0, &N::release, notesAny },
    { "the truncation once", 0x89396910, &N::truncateOnce, notesAny },
    { "the tone's ended routine", N::kEndedRoutine, &N::ended, notesTone },
    { "the tone's truncated routine", N::kTruncatedRoutine, &N::truncated, notesTone },
};

template <size_t K>
std::string notesOneArgEvent(NoteSides& s, std::mt19937& rng) {
    const NotesOneArg& e = kOneArg[K];
    const uint32_t arg = e.draw(s, rng);
    ++s.n.byEvent[K];
    return notesOnBoth(s, e.routine, { arg }, e.name, false, [&] {
        (s.port.*e.mine)(arg, s.parts);
        return 0u;
    });
}

std::string notesConstructEvent(NoteSides& s, std::mt19937& rng) {
    const uint32_t note = notesAny(s, rng), part = notesWord(rng), kind = notesWord(rng);
    ++s.n.byEvent[7];
    return notesOnBoth(s, 0x89396520, { note, part, kind }, "the constructor", false, [&] {
        s.port.construct(note, part, kind, s.parts);
        return 0u;
    });
}

std::string notesNoteOnEvent(NoteSides& s, std::mt19937& rng) {
    const uint32_t note = notesAny(s, rng), partner = rng() % 3 == 0 ? 0 : notesAny(s, rng);
    const uint32_t onByte = notesWord(rng);
    ++s.n.byEvent[8];
    s.n.partners += partner != 0;
    return notesOnBoth(s, 0x893965e0, { note, partner, onByte }, "the note-on", false, [&] {
        s.port.noteOn(note, partner, onByte, s.parts);
        return 0u;
    });
}

// The start and its copy, given a note they don't read
template <uint32_t Routine, size_t K>
std::string notesStartEvent(NoteSides& s, std::mt19937& rng) {
    const uint32_t note = notesAny(s, rng);
    ++s.n.byEvent[K];
    return notesOnBoth(s, Routine, { note }, "the start", false, [&] {
        s.port.start(s.parts);
        return 0u;
    });
}

std::string notesPlaceEvent(NoteSides& s, std::mt19937& rng) {
    const uint32_t n = rng() % 4 ? rng() % N::kNotes : (uint32_t)rng();
    ++s.n.byEvent[11];
    return notesOnBoth(s, 0x893964f0, { kNoteBytes, n }, "the placement", true, [&] { return s.port.addressOf(n); });
}

std::string notesPositionEvent(NoteSides& s, std::mt19937& rng) {
    const uint32_t note = notesAny(s, rng), osc = notesWord(rng);
    ++s.n.byEvent[12];
    return notesOnBoth(s, 0x89396950, { note, osc }, "the position", true,
                       [&] { return s.port.position(note, osc, s.parts); });
}

std::string notesInitEvent(NoteSides& s, std::mt19937&) {
    ++s.n.byEvent[13];
    return notesOnBoth(s, 0x89396960, {}, "the hook's initializer", false, [&] {
        s.port.staticInit();
        return 0u;
    });
}

const NoteEvent kEvents[] = {
    notesOneArgEvent<0>, notesOneArgEvent<1>, notesOneArgEvent<2>, notesOneArgEvent<3>, notesOneArgEvent<4>,
    notesOneArgEvent<5>, notesOneArgEvent<6>, notesConstructEvent, notesConstructEvent, notesNoteOnEvent,
    notesNoteOnEvent, notesStartEvent<0x89396630, 9>, notesStartEvent<0x89396640, 10>, notesPlaceEvent,
    notesPositionEvent, notesInitEvent,
};
} // namespace

std::span<const NoteEvent> noteEvents() { return kEvents; }
#endif
