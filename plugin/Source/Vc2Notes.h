#pragma once
// The VC-2 part's notes W (v275, step 4.3's third version): rom.4's 24 notes of 0x744 bytes from 0x8998a51c, one by
// each voice's number (0x893964f0), each the tone's input (`Vc2ToneInput`) with its tone, its legato partner and four
// bytes; the hook word 0x8998a514 beside, and a float there that nothing reads.  The constructor 0x89396520 (a tone of
// the pool's taken, given the note's two routines), the note-on 0x893965e0, the end 0x89396590, the start 0x89396630,
// the block pass 0x89396650 and 0x893967a0 (one code twice: the voice's settings and its update), the release
// 0x893968b0, the truncation once 0x89396910 and the position 0x89396950; the two routines a tone is given, 0x893964d0
// (the hook word's routine) and 0x893964e0 (the tone undone), and the hook's initializer 0x89396960.  The links are
// rom.4's addresses.  0x89396640 is a copy of the start nothing calls, 0x89396940 an empty routine.  The tone's methods
// through `Vc2NoteParts`.  Bit for bit: xt_vc2notes against rom.4's routines on its own notes, every tone method
// hooked.  HARDWARE_FIRMWARE.md, "The VC-2 part's notes".  The notes' task.
#include "Vc2ToneInput.h"
#include <array>
#include <cstdint>
#include <span>

// A note W as rom.4 lays it out
struct Vc2NoteWords {
    Vc2ToneInput input {};              // +0x000: what the conversion writes and the tone's note-on reads
    std::array<uint32_t, 14> rest {};   // +0x700…+0x737: not touched by the note's routines
    uint32_t tone = 0;                  // +0x738: its tone, by address
    uint32_t partner = 0;               // +0x73c: the note a legato takes from, 0 none
    uint8_t onByte = 0;                 // +0x740: the note-on's third argument
    uint8_t part = 0;                   // +0x741: the part node's first byte, the tone's id's low half
    uint8_t kind = 0;                   // +0x742: the voice's kind, the id's high half
    uint8_t truncated = 0;              // +0x743: 1 once truncated at once
};
static_assert(sizeof(Vc2NoteWords) == 0x744, "rom.4's layout, a note W");

// The tone's block setters the block pass calls, by what each sets
enum class Vc2NoteBlock : int32_t {
    oscSet,     // 0x893b3350: both OSC voices' settings (the input's +0x0c)
    osc,        // 0x893b3110, by the OSC's number
    mix,        // 0x893b32d0
    cosm,       // 0x893b3240, by the COSM's number
    envelope,   // 0x893b31f0, by the envelope's number
    lfo,        // 0x893b31a0, by the LFO's number
    tva,        // 0x893b3310
};

// One setter's call in a pass: which, the number it is given (0 for the setters that take none) and the block's offset
// in the note
struct Vc2NoteBlockRow {
    Vc2NoteBlock which;
    uint32_t n;
    uint32_t offset;
};

// What the note's routines call on a tone, given by its address
class Vc2NoteParts {
public:
    virtual ~Vc2NoteParts() = default;
    virtual uint32_t make(uint32_t id) = 0;                         // 0x893b2db0(0, id): a tone of the pool's, built
    virtual void giveRoutines(uint32_t tone, uint32_t ended, uint32_t truncated) = 0;   // its +0x1b14 and +0x1b18
    virtual int32_t serial(uint32_t tone) = 0;                      // its +8
    virtual bool endTest(uint32_t tone) = 0;                        // its vtable's +0x60, 0x893b4040 (Vc2Tone::held)
    virtual void unmake(uint32_t tone) = 0;                         // 0x893b2fc0(tone, 3): back to the pool
    virtual void truncateAndWait(uint32_t tone) = 0;                // 0x893b3bc0
    virtual void truncateAtOnce(uint32_t tone) = 0;                 // 0x893b3c20
    virtual void noteOn(uint32_t tone, const Vc2NoteWords& note) = 0;   // 0x893b3390(tone, the note's input)
    virtual void start() = 0;                                       // 0x893b1cd0: the part's lists of tones
    virtual void setBlock(uint32_t tone, const Vc2NoteBlockRow& row, const uint32_t* block) = 0;
    virtual void keyOff(uint32_t tone) = 0;                         // 0x893b3ad0
    virtual uint32_t position(uint32_t tone, uint32_t osc) = 0;     // 0x893b3fd0
    virtual void call(uint32_t routine, uint32_t tone) = 0;         // the hook word's routine, given the tone
};

class Vc2Notes {
public:
    static constexpr int kNotes = 24;
    static constexpr uint32_t kHookAt = 0x8998a514, kNotesAt = 0x8998a51c;   // [FW] the words, the notes
    static constexpr uint32_t kNoHook = 0x893964c0;        // [FW] the hook's first routine, which does nothing
    static constexpr uint32_t kEndedRoutine = 0x893964d0;  // [FW] the routines a constructor gives its tone
    static constexpr uint32_t kTruncatedRoutine = 0x893964e0;
    static constexpr uint32_t kNoSerial = 0xffffffff;      // [FW] the input's +4 with no partner

    uint32_t addressOf(uint32_t n) const;   // 0x893964f0, of any n
    Vc2NoteWords& at(uint32_t note) { return notes[(note - kNotesAt) / sizeof(Vc2NoteWords)]; }
    uint32_t addressOf(const void* word) const;   // where a byte of the notes is in rom.4's memory

    void staticInit();                                                          // 0x89396960
    void construct(uint32_t note, uint32_t part, uint32_t kind, Vc2NoteParts& parts);   // 0x89396520
    void noteOn(uint32_t note, uint32_t partner, uint32_t onByte, Vc2NoteParts& parts);  // 0x893965e0
    void end(uint32_t note, Vc2NoteParts& parts);                               // 0x89396590, 0 for none
    void start(Vc2NoteParts& parts);                                            // 0x89396630
    void setBlocks(uint32_t note, Vc2NoteParts& parts);                         // 0x89396650, 0x893967a0
    void release(uint32_t note, Vc2NoteParts& parts);                           // 0x893968b0
    void truncateOnce(uint32_t note, Vc2NoteParts& parts);                      // 0x89396910
    uint32_t position(uint32_t note, uint32_t osc, Vc2NoteParts& parts);        // 0x89396950
    void ended(uint32_t tone, Vc2NoteParts& parts);                             // 0x893964d0
    void truncated(uint32_t tone, Vc2NoteParts& parts);                         // 0x893964e0

    uint32_t hook = 0;     // 0x8998a514: the routine a tone's end reaches (the notes' task's, 0x893901a0)
    uint32_t unread = 0;   // 0x8998a518: a float 0x89396970 writes, which no code reads
    std::array<Vc2NoteWords, kNotes> notes {};

private:
    void pass(uint32_t note, std::span<const Vc2NoteBlockRow> rows, Vc2NoteParts& parts);
};
