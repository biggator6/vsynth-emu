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

class Vc2ToneShared;
class Vc2WaveMemory;
class Vc2WaveSets;
class Vc2WaveStore;
struct Vc2MatrixGroups;

// What C's routines are given beside C: the voice's +0x34 (its key table, source, part, number and three bytes), its
// +0x53, and a word (0x893a76d0's the legato's struct, the retune's rom.4's 0x8953fb2c, else 0)
struct Vc2SetCall {
    uint32_t channelAt = 0;
    uint32_t byteAt = 0;
    uint32_t word = 0;
};

// What a child's constructor is given: the object, C's call, its block in W, the note's M, the kind and the index
struct Vc2SetChildCall {
    uint32_t object;
    Vc2SetCall call;
    uint32_t noteAt;
    uint32_t matrix;
    uint32_t kind;
    uint32_t index;
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

// The clock object's words a tempo is made of (rom.4's 0x8995cb40, v282): its source (+0x40, 0 internal), the internal
// tempo's two words (+0x10, +0x14) and the outside clock's period (+0x48)
struct Vc2SetClock {
    uint8_t external = 0;
    int32_t numerator = 0;
    int32_t denominator = 0;
    uint16_t period = 0;
};

// A voice's glide key's words the OSC blocks read (v391): Vc2KeyGlide's key table (+0), its last POLY key and whether a
// key was held then (+0x1a, +0x1b), its last MONO key and the same (+0x20, +0x21)
struct Vc2SetGlide {
    uint32_t table = 0;
    uint8_t prevPoly = 0, prevPolyHeld = 0, prevMono = 0, prevMonoHeld = 0;
};

// What C's TVA reads past the part's words (v285): the flag of the part's CC 7 and 11 on the level (rom.4's byte
// 0x89997fcc), and the part table's trailer at 0x89e4b07c: a gain in sixths of an octave, a rate in hundredths of
// 44.1 kHz's samples and a route's flag
struct Vc2SetTvaGlobals {
    uint8_t controls = 0;   // 0x89997fcc
    int8_t gain = 0;        // 0x89e4b07c
    int16_t rate = 0;       // 0x89e4b07e
    uint8_t route = 0;      // 0x89e4b080
};

// What C, M and C's children read past their own words: the voice by a call, an object's first byte, K's channel state,
// the channel's bend and controllers (its +0x84 + number), the part's byte by kind (rom.4's 0x89998a14 + 16 × part +
// kind), the part's three words the common takes (0x89997090 + 6 × part, v281); the part's five words an envelope
// takes by kind and index (0x89e2765c + 2080 × part + 130 × kind + 10 × index), the clock, and the halfword at
// `offset` of the sums a Matrix group's reader (by rom.4's address) gives for M (v282)
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
    virtual std::array<uint16_t, 5> envelopeOf(uint32_t part, uint32_t kind, uint32_t index) = 0;
    // the part's five words an LFO takes (0x89e2f85c + 800 × part + 50 × kind + 10 × index, v283)
    virtual std::array<uint16_t, 5> lfoOf(uint32_t part, uint32_t kind, uint32_t index) = 0;
    // the part's eleven words the mix takes (0x89e32a60 + 352 × part + 22 × kind, v284)
    virtual std::array<uint16_t, 11> mixOf(uint32_t part, uint32_t kind) = 0;
    // the TVA's (v285): the part's twelve words (0x89e4987c + 384 × part + 24 × kind), the shared velocity curve
    // (0x893a7fb0), the note's M's controller `n` (M + 0x178 + n: CC 7, 11, 10, 93), the globals
    virtual std::array<uint16_t, 12> tvaOf(uint32_t part, uint32_t kind) = 0;
    virtual int velocityCurve(int velocity, int sens, int curve) = 0;
    virtual uint8_t matrixControl(uint32_t matrix, uint32_t n) = 0;
    virtual Vc2SetTvaGlobals tvaGlobals() = 0;
    virtual Vc2SetClock clock() = 0;
    virtual int16_t matrixSum(uint32_t reader, uint32_t matrix, uint32_t offset) = 0;
    // The COSM inputs' (v390): the part's 42 bytes for one (0x89e2003c + 0x540 × part + 84 × kind + 42 × index, each a
    // word as rom.4's constructor multiplies them), the routines the COSM converters share, and the sums a Matrix
    // group's reader (by rom.4's address) returns for M, as it returns them
    virtual std::array<uint16_t, 21> cosmInputOf(uint32_t part, uint32_t kind, uint32_t index) = 0;
    virtual const Vc2ToneShared& shared() = 0;
    virtual const int16_t* matrixGroup(uint32_t reader, uint32_t matrix) = 0;
    // The OSC blocks' (v391): the parts' OSC table (0x89e371e0: 0x1020 a part, its 32-byte header, then 0x100 a kind and
    // 0x80 an OSC; the systems' 0x3c bytes after the 16 parts), an OSC's step count (0x89989c84 + 64 × part + 4 × kind
    // + 2 × OSC, Vc2PartRows::steps), the glide key a voice's +0x38 names and a key table's Hold (+0x11c), the legato
    // struct's +1 and +2 that a make's call names, M's bend (+0x174), the Matrix's groups for an OSC by its four getters,
    // the pitch source's ratio (0x89394a40 on 0x89989748) (v391's `unportedOscKind`, the kinds' routines the port
    // hadn't, went with PCM's start, v397); v343: the step count as it was, counted on by one where it lies
    virtual std::array<uint8_t, 32> oscHeader(uint32_t part) = 0;
    virtual std::array<uint8_t, 128> oscRecord(uint32_t part, uint32_t kind, uint32_t osc) = 0;
    virtual std::array<uint8_t, 60> oscSystems() = 0;
    virtual uint16_t nextOscStep(uint32_t part, uint32_t kind, uint32_t osc) = 0;
    virtual Vc2SetGlide glideAt(uint32_t glide) = 0;
    virtual uint8_t holdOf(uint32_t keys) = 0;
    virtual std::array<uint8_t, 2> legatoOf(uint32_t word) = 0;
    virtual int32_t matrixBend(uint32_t matrix) = 0;
    virtual Vc2MatrixGroups oscGroups(uint32_t matrix, uint32_t osc) = 0;
    virtual float pitchSource() = 0;
    // PCM's start's (v397): the wave store (v398: Vc2WaveStore, rom.4's at 0x8997c52c) and the wave memory its records
    // and blocks lie in, by rom.4's address (VariPhrase's words read a wave's events there, v396's waveHalf until
    // v398); and the wave sets a set's index is looked up in (v399: Vc2WaveSets, their buffers written)
    virtual const Vc2WaveStore& waveStore() = 0;
    virtual const Vc2WaveMemory& waveMemory() = 0;
    virtual Vc2WaveSets& waveSets() = 0;
    // LITE's words' (v395): a part's Time Trip under way, its trip object's +8 (Vc2TripObject::moving) by the part table
    // 0x8993d040
    virtual uint32_t tripMoving(uint8_t part) = 0;
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
