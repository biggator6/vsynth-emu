#pragma once
// The VC-2 part's notes' Matrix objects M (v280, step 4.3's eighth version, with Vc2Sets): rom.4's 24 of 0x184 bytes
// from 0x89e4741c word for word, one by a voice's number (0x893a5d80); the constructor 0x893a5db0 and the update
// 0x893a6010 from the part's record in the Matrix's settings G (v247) and the channel's state its key table names.
// M's end 0x893a5fe0 and copy 0x893a61f0 are bare returns, so the port has none.  The reads through Vc2SetReads.  Bit for
// bit: xt_vc2sets.  HARDWARE_FIRMWARE.md, "The VC-2 part's conversion sets and the notes' Matrix".  The notes' task.
#include "Vc2MatrixSetup.h"
#include "Vc2PoolOf.h"
#include <array>
#include <cstdint>

// What C's routines are given beside C: the voice's +0x34 (its key table, source, part, number and three bytes), its
// +0x53, and a word (0x893a76d0's the legato's struct, the retune's rom.4's 0x8953fb2c, else 0)
struct Vc2SetCall {
    uint32_t channelAt = 0;
    uint32_t byteAt = 0;
    uint32_t word = 0;
};

// The voice's 16 bytes a call's first word points at, its +0x34…+0x43 (Vc2VoiceNode)
struct Vc2SetVoice {
    uint32_t keys = 0;     // +0x34: its key table K
    uint32_t source = 0;   // +0x38
    uint32_t part = 0;     // +0x3c: its part, by address, the number its first byte
    uint8_t number = 0;    // +0x40: the voice's
    uint8_t key = 0;       // +0x41
    uint8_t byte42 = 0;    // +0x42: its portamento key's
    uint8_t byte43 = 0;    // +0x43: the velocity or word the voice took
};

// What C, M and C's children read past their own words: the voice by a call, an object's first byte, K's channel state,
// the channel's bend and controllers (its +0x84 + number), the part's byte by kind (rom.4's 0x89998a14 + 16 × part +
// kind), the part's three words the common takes (0x89997090 + 6 × part, v281)
class Vc2SetReads {
public:
    virtual ~Vc2SetReads() = default;
    virtual Vc2SetVoice voiceAt(uint32_t channelAt) = 0;
    virtual uint8_t firstByte(uint32_t object) = 0;
    virtual uint32_t channelOf(uint32_t keys) = 0;
    virtual int16_t bendOf(uint32_t channel) = 0;
    virtual uint8_t controlOf(uint32_t channel, uint32_t number) = 0;
    virtual uint8_t modeOf(uint32_t part, uint32_t kind) = 0;
    virtual std::array<uint16_t, 3> commonOf(uint32_t part) = 0;
};

struct Vc2NoteMatrixWords {
    Vc2MatrixWords m {};                    // +0x000…+0x167 (Vc2Matrix.h)
    Vc2MatrixLinks links {};                // +0x168: K's channel state; +0x16c the part
    uint16_t bendUp = 0, bendDown = 0;      // +0x170, +0x172: the record's head, the Bend Ranges; 0 for kind ≠ 0
    int32_t bend = 0;                       // +0x174: the channel's bend by them
    std::array<uint8_t, 5> controls {};     // +0x178…+0x17c: the channel's CC 7, 11, 10, 93 and 91
    std::array<uint8_t, 3> unread17d {};
    std::array<uint8_t, 4> cleared {};      // +0x180…+0x183: 0 at each pass
};
static_assert(sizeof(Vc2NoteMatrixWords) == 0x184, "rom.4's layout, a note's Matrix object");

// What a note's M is made from: the Matrix's settings G, and the part's own M (rom.4's static at 0x89997108) whose head
// takes the note's
struct Vc2SetMatrixIn {
    const Vc2MatrixSettings& g;
    Vc2MatrixWords& partM;
    const Vc2MatrixLinks& partLinks;
};

// [FW] the pool at 0x89e4741c, its placement 0x893a5d80(0x184, number)
class Vc2NoteMatrices : public Vc2PoolOf<Vc2NoteMatrixWords, 24, 0x89e4741c> {
public:
    void construct(uint32_t m, const Vc2SetCall& call, uint32_t kind, const Vc2SetMatrixIn& in,
                   Vc2SetReads& reads);                                                          // 0x893a5db0
    void update(uint32_t m, const Vc2SetCall& call, uint32_t kind, const Vc2SetMatrixIn& in,
                Vc2SetReads& reads);                                                             // 0x893a6010

private:
    // Both routines' pass from the part's record and the channel's state on
    void fromPart(Vc2NoteMatrixWords& w, uint8_t part, uint32_t kind, const Vc2SetMatrixIn& in, Vc2SetReads& reads);
};
