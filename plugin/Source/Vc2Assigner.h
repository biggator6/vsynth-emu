#pragma once
// The VC-2 part's assigner, its first part (v277, step 4.3's fifth version): rom.4's routines that start the voices
// prepared for a note, on the voices and lists (Vc2PartVoices).  The start 0x89390860: for each prepared voice, a legato
// partner of its message's key table, part and kind among the sounding voices, the voice's +0x3c virtual, its note's
// note-on, the cost against the budget (the oldest voices of kind 0 stopped until it fits) and its note's start; then
// the prepared list onto the sounding list's front.  The class builder 0x893969e0 (a voice taken and made a note's, then
// the start), the partner alone 0x89390720 (a copy of the start's, which nothing calls), the matching test 0x893913a0,
// mono's two retunes 0x89390b20 and 0x89390b90 (the second a copy nothing calls), the truncation of a part's voices
// 0x893907d0 and a voice's 0x89391a50 with its two parts 0x89391a90 and 0x89391aa0.  A message is a portamento key P
// (Vc2Keys), its +0 its key table, which a voice keeps at its +0x34.  The voice's virtuals, its note's routines, the
// update queue's wait and a message's table through Vc2AssignParts.  Bit for bit: xt_vc2assign against rom.4's own
// routines on its own voices.  HARDWARE_FIRMWARE.md, "The VC-2 part's assigner".  The notes' task.
#include "Vc2PartVoices.h"

// What a voice being prepared is told of its legato partner: the stack struct rom.4 passes the voice's +0x3c
struct Vc2Legato {
    uint8_t found = 0;    // +0: a partner
    uint8_t marked = 0;   // +1: its age word's bit 29
    uint8_t onByte = 0;   // +2: the partner's note's +0x740, else the start's byte
};

// What the start and the class builder are given: the message, the part, a byte, a word passed on, and the kind a
// partner's note must be
struct Vc2AssignCall {
    uint32_t message = 0;
    uint32_t part = 0;
    uint32_t onByte = 0;
    uint32_t passed = 0;
    uint32_t kind = 0;
};

// What the assigner calls and reads beyond the voices
class Vc2AssignParts : public Vc2VoiceParts {
public:
    // The voice's vtable's +0x3c (a note's 0x89397130): its note, tone and conversion made from the call; it sets the
    // voice's +0x4c
    virtual void prepare(uint32_t voice, const Vc2AssignCall& call, const Vc2Legato& legato) = 0;
    virtual void retune(uint32_t voice, uint32_t message, uint32_t part, uint32_t value) = 0;   // its +0x18
    virtual void noteOn(uint32_t note, uint32_t partner, int32_t onByte) = 0;    // 0x893965e0
    virtual void startNote(uint32_t note) = 0;                                    // 0x89396630
    virtual void truncateOnce(uint32_t note) = 0;                                 // 0x89396910
    virtual int8_t onByteOf(uint32_t note) = 0;                                   // the note's +0x740
    virtual uint32_t tableOf(uint32_t message) = 0;                               // a message's +0, its key table
    virtual void waitQueue(uint32_t queue, uint32_t voice) = 0;                   // 0x893951b0
    // v278: the voice's +0x30 virtual (a note's key-off), a part's settings byte (0x89995390 + 32 × part), the part's
    // parameters' message 0x8939b050 (0x89996120 + 24 × part), an object's first byte (a message's part, a node's
    // channel), and the OS clock in ms (0x893018d0)
    virtual void keyOff(uint32_t voice) = 0;
    virtual uint8_t partSetting(uint32_t part, uint32_t offset) = 0;
    virtual void readParameters(uint32_t part) = 0;
    virtual uint8_t firstByte(uint32_t object) = 0;
    virtual int32_t clock() = 0;
    // v305: the voice's +0x48 virtual (a note's change test 0x893972d0, the plain voice's 0x89391720 answering 0),
    // given its +0x3c; its answer unread
    virtual void changed(uint32_t voice, uint32_t part) = 0;
};

struct Vc2KeyTable;
class Vc2Keys;

// What the part node's note start is given beyond the key table: the node, the message, a value its key is moved by,
// and a word passed on
struct Vc2NodeNote {
    uint32_t node = 0;
    uint32_t table = 0;
    uint32_t message = 0;
    int32_t value = 0;
    uint32_t passed = 0;
};

// A partner and whether its age word's bit 29 is set
struct Vc2Partner {
    uint32_t voice = 0;
    uint32_t marked = 0;
};

class Vc2Assigner {
public:
    static constexpr uint32_t kQueuesAt = 0x89989cc4, kQueueBytes = 36;   // [FW] the update queues, by a voice's +0x0c
    static constexpr uint32_t kNoteVtable = 0x898cd3dc;                   // [FW] a note's voice's class

    void start(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts);    // 0x89390860
    void build(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts);    // 0x893969e0
    Vc2Partner partner(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts);   // 0x89390720
    bool matching(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts);        // 0x893913a0
    // 0x89390b20 on the sounding voices of a note not of kind 1, 0x89390b90 of kind 1
    void retune(const Vc2AssignCall& call, uint32_t value, bool kindOne, Vc2PartVoices& v, Vc2AssignParts& parts);
    void truncateSame(const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts);    // 0x893907d0
    void truncateVoice(uint32_t voice, Vc2PartVoices& v, Vc2AssignParts& parts);              // 0x89391a50
    void clearOrders(uint32_t voice, Vc2PartVoices& v);                                       // 0x89391aa0
    void setState(uint32_t voice, uint32_t state, Vc2PartVoices& v);                          // 0x89391a90

    // The second part (v278, Vc2AssignRelease.cpp): a key's release on a voice by its key table, and the pedals' lifts
    void release(uint32_t voice, const Vc2KeyTable& keys, Vc2PartVoices& v, Vc2AssignParts& parts);   // 0x89391760
    void liftHold(uint32_t voice, Vc2PartVoices& v, Vc2AssignParts& parts);                          // 0x89391820
    void liftSostenuto(uint32_t voice, Vc2PartVoices& v, Vc2AssignParts& parts);                     // 0x89391880
    // and over the sounding voices of a key table, at `table`
    void noteOff(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts);       // 0x89390c00
    void holdOff(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts);       // 0x89390cd0
    void sostenutoOff(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts);  // 0x89390d10
    void truncateTable(uint32_t table, Vc2PartVoices& v, Vc2AssignParts& parts);                 // 0x89390d50
    void releaseBut(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts);    // 0x89390da0
    void releaseAll(uint32_t table, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts);    // 0x89390e20
    // The part node's note start and off (Vc2AssignNotes.cpp): its portamento keys by its key table's source and its
    // channel, the part's mode, then the start, a build, a truncation or mono's retune
    void nodeNoteOn(const Vc2NodeNote& n, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts);   // 0x89396a80
    void nodeNoteOff(const Vc2NodeNote& n, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts);  // 0x89396c90
    // The listeners' run (v305, Vc2AssignRelease.cpp): each sounding voice whose key table's channel state is
    // `state`, its +0x48 virtual
    void notify(uint32_t state, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts);   // 0x89390ea0

private:
    bool sameNote(uint32_t voice, const Vc2AssignCall& call, Vc2PartVoices& v, Vc2AssignParts& parts);
    void splice(Vc2PartVoices& v);
};
