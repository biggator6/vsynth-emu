#pragma once
// The VC-2 part's voices and their lists (v272, step 4.3's first part): rom.4's 24 voices V (0x60 bytes each, at the
// addresses of its table 0x895354b0), the four lists' heads (0x50 bytes each from 0x89982594: sounding, free, prepared,
// reserved), the lists' table 0x899826e0 and the words beside it (the reserve's count, the CPU budget, a voice's cost
// offset), and the next note's age 0x8998270c, every word rom.4's, a link a voice's or a head's address.  The heads'
// static initializer 0x89391450, the voices' init 0x89391ba0 and the lists' 0x89391270; the take 0x893905a0 (the free
// list's first, else the sounding voice of kind 0 with the lowest age) and the steal's two parts 0x893906c0 and
// 0x89390700; the three moves 0x893919b0 (to prepared), 0x893918e0 (to free) and 0x89391930 (to reserved); the reserve
// 0x893911b0; the ended scan 0x89390ee0; stop all 0x89390f40; a voice's cost 0x89390610, a list's 0x89390630 and the
// sounding list's count and cost 0x893912e0; the budget's setters 0x89391240 and 0x89391260 and its getter 0x893912d0;
// and the prepared list's test 0x89390850.  What they call into a note, its tone and the voice's virtuals through
// Vc2VoiceParts.  Bit for bit: xt_vc2voices against rom.4's routines in Sh4Interp, those calls hooked.  The notes'
// task's (HARDWARE_FIRMWARE.md, "The VC-2 part's voices and their lists").
#include <array>
#include <cstdint>

// What a list's head and a voice share, as rom.4 lays them out: the base class's 0x50 bytes
struct Vc2VoiceNode {
    uint32_t queueNext = 0, queuePrev = 0;   // +0x00, +0x04: the update queue's node's links (0x89394f30), itself out of one
    uint32_t queued = 0;                     // +0x08: the queue's flag
    uint8_t queue = 0;                       // +0x0c: which queue (0x89989cc4 + 36 × this)
    uint8_t unread0d[3] {};
    uint32_t vtable = 0;                     // +0x10
    uint32_t prev = 0, next = 0;             // +0x14, +0x18: its list's links
    uint32_t age = 0;                        // +0x1c: where its age is, kZeroAge's word or its +0x20
    std::array<uint32_t, 2> order {};        // +0x20, +0x24: its ages as a note (−1 prepared; 0x89391ae0's count)
    uint32_t after = 0;                      // +0x28: the voice after it by number, the last's the first
    uint8_t state = 0;                       // +0x2c: 0 or 1 sounding, kFreeState, kReservedState
    uint8_t released = 0;                    // +0x2d
    uint8_t byte2e = 0, byte2f = 0;          // +0x2e, +0x2f: the note's
    uint16_t updated = 0;                    // +0x30: bit 0 an update asked for
    uint16_t unread32 = 0;
    uint32_t channel = 0;                    // +0x34: its channel state
    uint32_t source = 0;                     // +0x38: the note's
    uint32_t part = 0;                       // +0x3c
    uint8_t number = 0;                      // +0x40: its index in the table
    uint8_t key = 0, byte42 = 0, byte43 = 0;   // +0x41…+0x43: the note's
    uint32_t leave = 0;                      // +0x44: what a move runs first, kDestroyOnLeave or kStayOnLeave
    uint8_t held = 0;                        // +0x48: the key's and the pedals' bits
    uint8_t unread49[3] {};
    uint32_t note = 0;                       // +0x4c: its note W, 0 none
};
static_assert(sizeof(Vc2VoiceNode) == 0x50, "rom.4's layout, a list's head and a voice's +0x00…+0x50");

// A voice: the base's words and the note's class's (0x893969e0 builds it in place on a voice taken)
struct Vc2Voice {
    Vc2VoiceNode node {};
    uint8_t byte50 = 0, byte51 = 0, byte52 = 0, byte53 = 0;   // +0x50…+0x53: the note's class's (0, −1 at +0x52)
    uint32_t word54 = 0, word58 = 0;                         // +0x54, +0x58
    uint32_t set = 0;                                        // +0x5c: its conversion set C, 0 none
};
static_assert(sizeof(Vc2Voice) == 0x60, "rom.4's layout, a voice's 0x60 bytes");

// The words at 0x899826d4…0x899826fc, as rom.4 lays them out
struct Vc2VoiceGlobals {
    int32_t reserved = 0;               // 0x899826d4: the reserve's count
    float budget = 0.0f;                // 0x899826d8: the CPU budget a block's tones may cost
    float offset = 0.0f;                // 0x899826dc: what each tone's cost has added
    std::array<uint32_t, 4> lists {};   // 0x899826e0: the four lists' heads
    float unread0 = 0.0f, unread1 = 0.0f, unread2 = 0.0f;   // 0x899826f0…0x899826f8: the static initializer's, unread
};
static_assert(sizeof(Vc2VoiceGlobals) == 0x28, "rom.4's layout, 0x899826d4…0x899826fc");

// The update queue's node's constructor 0x89394f30 on a node given: its vtable, its links to itself
void vc2ConstructNode(Vc2VoiceNode& n, uint32_t self);

// What the voices call and read beyond themselves
class Vc2VoiceParts {
public:
    virtual ~Vc2VoiceParts() = default;
    virtual void destroy(uint32_t voice) = 0;      // its vtable's +0x24, which kDestroyOnLeave calls
    virtual uint32_t toneOf(uint32_t note) = 0;    // the note W's +0x738
    virtual int8_t kindOf(uint32_t note) = 0;      // W's +0x742
    virtual bool ended(uint32_t tone) = 0;         // the tone's vtable's +0x60 (Vc2Tone::held)
    virtual float cost(uint32_t tone) = 0;         // 0x893b4020
};

// A count and a sum, as the two getters write them
struct Vc2VoiceCount {
    int32_t count = 0;
    float sum = 0.0f;
};

class Vc2PartVoices {
public:
    static constexpr int kVoices = 24, kLists = 4;
    static constexpr int kSounding = 0, kFree = 1, kPrepared = 2, kReserved = 3;   // [FW] the lists' order at 0x899826e0
    static constexpr uint8_t kFreeState = 2, kReservedState = 3;      // [FW] a voice's +0x2c in those lists
    static constexpr uint32_t kHeadsAt = 0x89982594;                  // [FW] the four heads, kHeadBytes apart
    static constexpr uint32_t kHeadBytes = sizeof(Vc2VoiceNode);
    static constexpr uint32_t kGlobalsAt = 0x899826d4, kAgesAt = 0x8998270c;   // [FW] the words rom.4 keeps them at
    static constexpr uint32_t kZeroAge = 0x89535510;                  // [FW] the age of a voice not prepared
    static constexpr uint32_t kDestroyOnLeave = 0x89391740, kStayOnLeave = 0x89391750;   // [FW] the +0x44 routines
    static constexpr uint32_t kNodeVtable = 0x898cd340, kVoiceVtable = 0x898cd1a8;     // [FW] the node's, the voice's

    void staticInit();                              // 0x89391450: the heads built, the three unread floats
    void initVoices();                              // 0x89391ba0
    void initLists();                               // 0x89391270
    uint32_t take(Vc2VoiceParts& parts);            // 0x893905a0: a voice moved to prepared, 0 none
    uint32_t oldest(Vc2VoiceParts& parts);          // 0x893906c0: the sounding voice of kind 0 with the lowest age
    uint32_t stop(uint32_t voice, Vc2VoiceParts& parts);   // 0x89390700: freed if there is one
    void toPrepared(uint32_t voice, uint8_t state, Vc2VoiceParts& parts);   // 0x893919b0
    void toFree(uint32_t voice, Vc2VoiceParts& parts);       // 0x893918e0
    void toReserved(uint32_t voice, Vc2VoiceParts& parts);   // 0x89391930
    void reserve(int32_t count, int32_t offset, Vc2VoiceParts& parts);   // 0x893911b0
    void endedScan(Vc2VoiceParts& parts);           // 0x89390ee0
    void stopAll(Vc2VoiceParts& parts);             // 0x89390f40
    float cost(uint32_t voice, Vc2VoiceParts& parts);   // 0x89390610: its tone's and the offset
    float costOf(uint32_t list, Vc2VoiceParts& parts);  // 0x89390630: the voices', the first of kind 1 alone of its kind
    Vc2VoiceCount sounding(Vc2VoiceParts& parts);   // 0x893912e0: the sounding voices and their cost
    void setBudget(int32_t power);                  // 0x89391240: CPU Power Max for TG, ÷ 10^6
    void setOffset(int32_t power);                  // 0x89391260: CPU Power Offset for Voice, ÷ 10^7
    Vc2VoiceCount budget() const;                   // 0x893912d0: the reserve's count and the budget
    bool preparedAny();                             // 0x89390850

    // A head's or a voice's words by its address; a voice's address by its index
    Vc2VoiceNode& at(uint32_t address);
    uint32_t addressOf(int voice) const;
    Vc2Voice& voice(uint32_t address);   // a voice's whole words, by its address (a voice's, never a head's)
    uint32_t ageOf(uint32_t voice);      // the word its +0x1c points at

    std::array<Vc2Voice, kVoices> voices {};
    std::array<Vc2VoiceNode, kLists> heads {};
    Vc2VoiceGlobals g {};
    uint32_t ages = 0;   // 0x8998270c: the next note's age, 0 again when no voice sounds

private:
    void append(uint32_t voice, uint32_t list);
    void leave(uint32_t voice, Vc2VoiceParts& parts);
    void move(uint32_t voice, int list, uint8_t state, Vc2VoiceParts& parts);
    void stopped(uint32_t voice, int list, uint8_t state, Vc2VoiceParts& parts);
};
