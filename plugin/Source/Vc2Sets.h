#pragma once
// The VC-2 part's conversion sets C (v280, step 4.3's eighth version, with Vc2NoteMatrix): rom.4's 24 of 0x6c bytes
// from 0x89997fe8 word for word, one by a voice's number (0x893a76a0), each the note's kind and the part's byte by kind,
// its children and its M.  The make 0x893a76d0, end 0x893a7a20, conversion 0x893a7b20, release 0x893a7c50, update
// 0x893a7cc0, copy 0x893a7df0 and refresh 0x893a7f10, each a pass over the children in rom.4's order by a table of
// kinds (Vc2SetChild: the common, the two OSC blocks, the mix, 13 envelopes, 5 LFOs, the two COSM inputs, the TVA),
// whose routines are behind Vc2SetParts; the note's M is Vc2NoteMatrix's.  The note W's words C writes through
// Vc2Notes.  Bit for bit: xt_vc2sets.  HARDWARE_FIRMWARE.md, "The VC-2 part's conversion sets and the notes' Matrix".
// The notes' task.
#include "Vc2NoteMatrix.h"
#include "Vc2Notes.h"
#include <span>

// A kind of C's children: its routines, what its allocator is given, its pointers in C and its blocks in W.  Every
// routine is given all C has for it; which of those rom.4's own read (an allocator the index, a constructor M, the
// kind and the index) is each routine's, xt_vc2sets' table.
struct Vc2SetChild {
    uint32_t allocate, construct, end, convert, update, copy, release, refresh;   // [FW], 0 for none
    uint32_t bytes;         // the allocator's first argument
    uint32_t slot;          // its first pointer's offset in C
    uint32_t count;
    uint32_t noteAt;        // its first block's offset in W, and the next's after it
    uint32_t noteStride;
};

// The children's routines, each given its kind's row, and what C reads
class Vc2SetParts : public Vc2SetReads {
public:
    virtual uint32_t allocate(const Vc2SetChild& c, uint32_t number, uint32_t index) = 0;
    virtual void construct(const Vc2SetChild& c, const Vc2SetChildCall& call) = 0;
    virtual void end(const Vc2SetChild& c, uint32_t object) = 0;
    virtual void convert(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call, uint32_t noteAt) = 0;
    virtual void update(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) = 0;
    virtual void copy(const Vc2SetChild& c, uint32_t object, uint32_t noteAt) = 0;
    virtual void release(const Vc2SetChild& c, uint32_t object, uint32_t velocity, uint32_t noteAt) = 0;
    virtual uint32_t refresh(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) = 0;
};

struct Vc2SetWords {
    uint8_t kind = 0;                       // +0x00: the note's (W+0x742)
    uint8_t mode = 0;                       // +0x01: the part's byte by kind, into W+0 at each conversion and copy
    std::array<uint8_t, 2> unread02 {};
    std::array<uint32_t, 25> children {};   // +0x04…+0x67: by Vc2SetChild's slots, 0 for none
    uint32_t matrix = 0;                    // +0x68: the note's M
};
static_assert(sizeof(Vc2SetWords) == 0x6c, "rom.4's layout, a conversion set");

// [FW] the sets at 0x89997fe8, their placement 0x893a76a0(0x6c, number)
class Vc2Sets : public Vc2PoolOf<Vc2SetWords, 24, 0x89997fe8> {
public:
    // The children's kinds in rom.4's order (the common's first: the make's alone keeps it to kind 0)
    static std::span<const Vc2SetChild> children();

    void make(uint32_t set, const Vc2SetCall& call, uint32_t note, Vc2Notes& notes, const Vc2SetMatrixIn& in,
              Vc2SetParts& parts);                                                                    // 0x893a76d0
    void end(uint32_t set, Vc2SetParts& parts);                                                       // 0x893a7a20
    void convert(uint32_t set, const Vc2SetCall& call, uint32_t note, Vc2Notes& notes, Vc2SetParts& parts);
    void release(uint32_t set, uint32_t velocity, uint32_t note, Vc2SetParts& parts);                // 0x893a7c50
    void update(uint32_t set, const Vc2SetCall& call, const Vc2SetMatrixIn& in, Vc2SetParts& parts);  // 0x893a7cc0
    void copy(uint32_t set, uint32_t note, Vc2Notes& notes, Vc2SetParts& parts);                    // 0x893a7df0
    uint32_t refresh(uint32_t set, const Vc2SetCall& call, Vc2SetParts& parts);                      // 0x893a7f10

    Vc2NoteMatrices matrices {};
};
