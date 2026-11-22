#pragma once
// xt_vc2notes' shared parts (v275): the calls each side logs (every tone method the notes' routines reach, and the hook
// word's routines), each with the argument registers rom.4's routine is given; what they answer, drawn by one generator
// per side from one seed, so each side's n-th call gets the same; the port's parts, which read the tones' words where
// the draws wrote them in rom.4's memory and keep the two routines a constructor gives a tone; and the two sides.
// XT_VC2NOTES_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2Notes.h"
#include <functional>
#include <span>
#include <vector>

inline constexpr uint32_t kNotesTonesAt = 0x8999f524, kNotesToneBytes = 0x261c;    // [FW] the tones' pool
inline constexpr uint32_t kNotesToneSerial = 0x08, kNotesToneVtable = 0x18;         // [FW] a tone's +8 and vtable
inline constexpr uint32_t kNotesToneRoutines = 0x1b14;                               // [FW] its two routines' words
inline constexpr uint32_t kNotesToneClass = 0x898cd568, kNotesEndEntry = 0x60;      // [FW] its vtable, the end test
inline constexpr uint32_t kNotesTaskHook = 0x893901a0;                               // [FW] the notes' task's hook
inline constexpr int kNotesTones = 24;

enum NoteKind : int32_t {
    kNoteMake, kNoteEndTest, kNoteUnmake, kNoteTruncateWait, kNoteTruncateNow, kNoteOnTone, kNoteStart, kNoteSet,
    kNoteKeyOff, kNotePosition, kNoteHook, kNoteKinds
};

// A call as either side made it: what, the routine it reached and the argument registers it was given (r4, r5, r6;
// 0 past those the routine reads)
struct NoteCall {
    int32_t kind;
    uint32_t routine;
    std::array<uint32_t, 3> args;
    bool operator==(const NoteCall&) const = default;
};

// What the hooked calls answer: a generator per side, both seeded alike before each event
struct NoteAnswers {
    std::mt19937 rom, port;
    void restart(uint32_t seed);
};
// The n-th answer of `kind` from `rng`: a tone of the pool's for a make, a word for the end test and the position
uint32_t noteAnswer(int32_t kind, std::mt19937& rng);

// The routine each block setter is, and whether it takes a number before its block
struct NoteSetter {
    uint32_t routine;
    bool numbered;
};
NoteSetter noteSetterOf(Vc2NoteBlock which);
uint32_t noteToneAt(int k);

// The port's parts: the tones' words read from rom.4's memory, the calls logged, the routines a constructor gives a
// tone kept by the tone's number
class NotePortParts final : public Vc2NoteParts {
public:
    NotePortParts(Vc2Fw& fw, Vc2Notes& port, NoteAnswers& answers, std::vector<NoteCall>& log)
        : fw_(fw), port_(port), answers_(answers), log_(log) {}
    uint32_t make(uint32_t id) override;
    void giveRoutines(uint32_t tone, uint32_t ended, uint32_t truncated) override;
    int32_t serial(uint32_t tone) override { return (int32_t)fw_.cpu.rd32(tone + kNotesToneSerial); }
    bool endTest(uint32_t tone) override;
    void unmake(uint32_t tone) override;
    void truncateAndWait(uint32_t tone) override;
    void truncateAtOnce(uint32_t tone) override;
    void noteOn(uint32_t tone, const Vc2NoteWords& note) override;
    void start() override;
    void setBlock(uint32_t tone, const Vc2NoteBlockRow& row, const uint32_t* block) override;
    void keyOff(uint32_t tone) override;
    uint32_t position(uint32_t tone, uint32_t osc) override;
    void call(uint32_t routine, uint32_t tone) override;

    std::array<std::array<uint32_t, 2>, kNotesTones> routines {};   // each tone's +0x1b14 and +0x1b18

private:
    uint32_t logged(int32_t kind, uint32_t routine, std::array<uint32_t, 3> args);
    Vc2Fw& fw_;
    Vc2Notes& port_;
    NoteAnswers& answers_;
    std::vector<NoteCall>& log_;
};

struct NoteCounts {
    long events = 0;
    std::array<long, 14> byEvent {};
    std::array<long, kNoteKinds> calls {};
    long partners = 0, truncatedAlready = 0, ends = 0, endsAtOnce = 0;
};

// The two sides: rom.4's interpreter and its log, its hooks installed; the port's notes, parts, answers and log
struct NoteSides {
    explicit NoteSides(Vc2Fw& rig);
    Vc2Fw& fw;
    Vc2Notes port {};
    NoteAnswers answers {};
    std::vector<NoteCall> rom {}, mine {};
    NotePortParts parts;
    NoteCounts n {};
};

// An event on both sides: what differs, or nothing
using NoteEvent = std::function<std::string(NoteSides&, std::mt19937&)>;
std::span<const NoteEvent> noteEvents();
// The port's words into rom.4's memory with the tones' routines, both logs cleared, the answers seeded with `seed`
void notesPrepare(NoteSides& s, uint32_t seed);
// The port's words and the tones' routines against rom.4's, then the calls; empty when nothing differs
std::string notesDifference(NoteSides& s);
// rom.4's side: every tone method the notes reach and the hook's routines hooked to log into `log` and answer
void notesInstallRom(Vc2Fw& fw, NoteAnswers& answers, std::vector<NoteCall>& log);
// The first call the two logs differ at; empty when they don't
std::string notesCallsDifference(const std::vector<NoteCall>& rom, const std::vector<NoteCall>& port);
