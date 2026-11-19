// xt_vc2notes' two sides' fakes: see Vc2NotesCheck.h (v275).  rom.4's: every tone method the notes' routines reach,
// the block setters and the hook word's two routines, hooked to log the argument registers each reads and answer from
// rom.4's generator; the port's: its parts logging the registers rom.4's routine would be given and answering from
// theirs.  XT_VC2NOTES_MAIN only.
#ifdef XT_VC2NOTES_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2NotesCheck.h"

namespace {
constexpr uint32_t kEntryRoutine = 8;   // [FW] a vtable slot's routine, after its adjustment's word
constexpr uint32_t kUndo = 3;           // [FW] the destructor's flag the notes give

// A hooked routine of rom.4's: its kind and how many argument registers it reads, from r4
struct Hooked {
    uint32_t routine;
    int32_t kind;
    int regs;
};

// [FW] the tone methods the notes' routines reach, and the hook word's two routines
constexpr Hooked kHooked[] = {
    { 0x893b2db0, kNoteMake, 2 },        { 0x893b2fc0, kNoteUnmake, 2 },   { 0x893b3bc0, kNoteTruncateWait, 1 },
    { 0x893b3c20, kNoteTruncateNow, 1 }, { 0x893b3390, kNoteOnTone, 2 },   { 0x893b1cd0, kNoteStart, 0 },
    { 0x893b3ad0, kNoteKeyOff, 1 },      { 0x893b3fd0, kNotePosition, 2 }, { Vc2Notes::kNoHook, kNoteHook, 1 },
    { kNotesTaskHook, kNoteHook, 1 },
};

// [FW] the block setters by Vc2NoteBlock's order
constexpr NoteSetter kSetters[] = {
    { 0x893b3350, false }, { 0x893b3110, true }, { 0x893b32d0, false }, { 0x893b3240, true },
    { 0x893b31f0, true },  { 0x893b31a0, true }, { 0x893b3310, false },
};

constexpr const char* kKindNames[kNoteKinds] = { "make", "end test", "unmake", "truncation and wait",
                                                 "truncation at once", "note-on", "start", "block setter",
                                                 "key-off", "position", "hook" };

std::array<uint32_t, 3> registers(const Sh4& c, int regs) {
    std::array<uint32_t, 3> args {};
    for (int k = 0; k < regs; ++k) args[(size_t)k] = c.r[4 + k];
    return args;
}

void hookOne(Vc2Fw& fw, uint32_t routine, int32_t kind, int regs, NoteAnswers& answers, std::vector<NoteCall>& log) {
    fw.cpu.hook(routine, [=, &answers, &log](Sh4& c) {
        log.push_back({ kind, routine, registers(c, regs) });
        c.ret(noteAnswer(kind, answers.rom));
    });
}
} // namespace

void NoteAnswers::restart(uint32_t seed) {
    rom.seed(seed);
    port.seed(seed);
}

uint32_t noteToneAt(int k) { return kNotesTonesAt + kNotesToneBytes * (uint32_t)k; }

uint32_t noteAnswer(int32_t kind, std::mt19937& rng) {
    using Draw = uint32_t (*)(std::mt19937&);
    constexpr Draw none = [](std::mt19937&) { return 0u; };
    constexpr Draw tone = [](std::mt19937& g) { return noteToneAt((int)(g() % kNotesTones)); };
    constexpr Draw test = [](std::mt19937& g) { return g() % 3 == 0 ? 1u : g() % 6 == 0 ? (uint32_t)g() : 0u; };
    constexpr Draw word = [](std::mt19937& g) { return (uint32_t)g(); };
    constexpr std::array<Draw, kNoteKinds> kDraws = { tone, test, none, none, none, none, none, none, none, word,
                                                      none };
    return kDraws[(size_t)kind](rng);
}

NoteSetter noteSetterOf(Vc2NoteBlock which) { return kSetters[(size_t)which]; }

uint32_t NotePortParts::logged(int32_t kind, uint32_t routine, std::array<uint32_t, 3> args) {
    log_.push_back({ kind, routine, args });
    return noteAnswer(kind, answers_.port);
}

uint32_t NotePortParts::make(uint32_t id) { return logged(kNoteMake, kHooked[0].routine, { 0, id, 0 }); }

void NotePortParts::giveRoutines(uint32_t tone, uint32_t ended, uint32_t truncated) {
    routines[(tone - kNotesTonesAt) / kNotesToneBytes] = { ended, truncated };
}

bool NotePortParts::endTest(uint32_t tone) {
    const uint32_t slot = fw_.cpu.rd32(tone + kNotesToneVtable) + kNotesEndEntry;
    return logged(kNoteEndTest, fw_.cpu.rd32(slot + kEntryRoutine), { tone + fw_.cpu.rd32(slot), 0, 0 }) != 0;
}

void NotePortParts::unmake(uint32_t tone) { logged(kNoteUnmake, kHooked[1].routine, { tone, kUndo, 0 }); }
void NotePortParts::truncateAndWait(uint32_t tone) { logged(kNoteTruncateWait, kHooked[2].routine, { tone, 0, 0 }); }
void NotePortParts::truncateAtOnce(uint32_t tone) { logged(kNoteTruncateNow, kHooked[3].routine, { tone, 0, 0 }); }

void NotePortParts::noteOn(uint32_t tone, const Vc2NoteWords& note) {
    logged(kNoteOnTone, kHooked[4].routine, { tone, port_.addressOf(&note), 0 });
}

void NotePortParts::start() { logged(kNoteStart, kHooked[5].routine, {}); }

void NotePortParts::setBlock(uint32_t tone, const Vc2NoteBlockRow& row, const uint32_t* block) {
    const NoteSetter setter = noteSetterOf(row.which);
    const uint32_t at = port_.addressOf(block);
    logged(kNoteSet, setter.routine, setter.numbered ? std::array<uint32_t, 3> { tone, row.n, at }
                                                     : std::array<uint32_t, 3> { tone, at, 0 });
}

void NotePortParts::keyOff(uint32_t tone) { logged(kNoteKeyOff, kHooked[6].routine, { tone, 0, 0 }); }

uint32_t NotePortParts::position(uint32_t tone, uint32_t osc) {
    return logged(kNotePosition, kHooked[7].routine, { tone, osc, 0 });
}

void NotePortParts::call(uint32_t routine, uint32_t tone) { logged(kNoteHook, routine, { tone, 0, 0 }); }

void notesInstallRom(Vc2Fw& fw, NoteAnswers& answers, std::vector<NoteCall>& log) {
    for (const Hooked& h : kHooked) hookOne(fw, h.routine, h.kind, h.regs, answers, log);
    for (const NoteSetter& setter : kSetters)
        hookOne(fw, setter.routine, kNoteSet, setter.numbered ? 3 : 2, answers, log);
    hookOne(fw, fw.cpu.rd32(kNotesToneClass + kNotesEndEntry + kEntryRoutine), kNoteEndTest, 1, answers, log);
}

std::string notesCallsDifference(const std::vector<NoteCall>& rom, const std::vector<NoteCall>& port) {
    return vc2CallsDifference(rom, port, [](const NoteCall& c) {
        char t[96];
        std::snprintf(t, sizeof t, "the %s %08x (%08x %08x %08x)", kKindNames[c.kind], c.routine, c.args[0],
                      c.args[1], c.args[2]);
        return std::string(t);
    });
}
#endif
