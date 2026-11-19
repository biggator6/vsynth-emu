#pragma once
// The VC-2 part's note voice's class (v279, step 4.3's seventh version, with Vc2VoiceQueue): rom.4's six virtuals of
// the class 0x898cd3dc on a voice (Vc2PartVoices): the update +0x0c 0x893970b0, the retune +0x18 0x893971f0, the
// destroy +0x24 0x893973a0, the key-off +0x30 0x89397280, the prepare +0x3c 0x89397130 and the change test +0x48
// 0x893972d0; and the voice's words they set from a portamento key and its key table (Vc2Keys): the prepare's
// 0x89391ae0 and the retune's 0x89391b40 (both given the part in r6 and the byte in r7; the retune's leave the part
// unread, so the port isn't given it).  The note W's routines (v275), the conversion set C's (step 4.3's next
// item), the update queue's (Vc2VoiceQueue) and the OS's lock through Vc2VoiceClassParts.  Bit for bit: xt_vc2voices'
// fourth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's voices' virtuals and update queue".  The notes' task.
#include "Vc2PartVoices.h"

class Vc2Keys;

// What C's routines are given: the voice's +0x34 and +0x53, by address, and a word
struct Vc2SetCall {
    uint32_t channelAt = 0;
    uint32_t byteAt = 0;
    uint32_t word = 0;
};

// What the virtuals call beyond the voice
class Vc2VoiceClassParts {
public:
    virtual ~Vc2VoiceClassParts() = default;
    virtual void constructNote(uint32_t note, uint32_t part, uint32_t kind) = 0;    // 0x89396520
    virtual void endNote(uint32_t note) = 0;                                        // 0x89396590(W, 3)
    virtual void noteBlocks(uint32_t note) = 0;                                     // 0x89396650
    virtual void noteUpdate(uint32_t note) = 0;                                     // 0x893967a0
    virtual void noteRelease(uint32_t note) = 0;                                    // 0x893968b0
    virtual uint32_t placeSet(uint32_t number) = 0;                                 // 0x893a76a0(108, n)
    virtual void makeSet(uint32_t set, const Vc2SetCall& call, uint32_t note) = 0;   // 0x893a76d0
    virtual void endSet(uint32_t set) = 0;                                          // 0x893a7a20(C, 3)
    virtual void convertSet(uint32_t set, const Vc2SetCall& call, uint32_t note) = 0;    // 0x893a7b20
    virtual void releaseSet(uint32_t set, uint32_t velocity) = 0;                   // 0x893a7c50
    virtual void updateSet(uint32_t set, const Vc2SetCall& call) = 0;               // 0x893a7cc0
    virtual void copySet(uint32_t set, uint32_t note) = 0;                          // 0x893a7df0
    virtual uint32_t refreshSet(uint32_t set, const Vc2SetCall& call) = 0;          // 0x893a7f10
    virtual void lock() = 0;                                                        // 0x893018e0(0x8994fec8)
    virtual void unlock() = 0;                                                      // 0x893018f0(0x8994fec8)
    virtual void enqueue(uint32_t queue, uint32_t voice) = 0;                       // 0x89394fa0
    virtual void takeOff(uint32_t queue, uint32_t voice) = 0;                       // 0x89395120
    virtual void waitQueue(uint32_t queue, uint32_t voice) = 0;                     // 0x893951b0
    virtual uint8_t firstByte(uint32_t object) = 0;                                 // a part's number
    virtual uint32_t retuneWord() = 0;                                              // the word at 0x8953fb2c
};

// What the prepare virtual is given: the message, the part, the byte, and on the stack the legato's struct (by address),
// the word passed on and the kind
struct Vc2PrepareCall {
    uint32_t message = 0;
    uint32_t part = 0;
    uint32_t onByte = 0;
    uint32_t legatoAt = 0;
    uint32_t passed = 0;
    uint32_t kind = 0;
};

class Vc2VoiceClass {
public:
    static constexpr uint32_t kQueueAt = 0x89989cc4, kQueueBytes = 36;   // [FW] the update queue, by a voice's +0x0c

    void prepareWords(uint32_t voice, const Vc2PrepareCall& c, Vc2PartVoices& v, Vc2Keys& keys);           // 0x89391ae0
    void retuneWords(uint32_t voice, uint32_t message, uint32_t onByte, Vc2PartVoices& v, Vc2Keys& keys);  // 0x89391b40
    void update(uint32_t voice, Vc2PartVoices& v, Vc2VoiceClassParts& parts);                              // +0x0c
    void retune(uint32_t voice, uint32_t message, uint32_t onByte, Vc2PartVoices& v, Vc2Keys& keys,
                Vc2VoiceClassParts& parts);                                                                // +0x18
    void destroy(uint32_t voice, Vc2PartVoices& v, Vc2VoiceClassParts& parts);                             // +0x24
    void keyOff(uint32_t voice, Vc2PartVoices& v, Vc2VoiceClassParts& parts);                              // +0x30
    void prepare(uint32_t voice, const Vc2PrepareCall& c, Vc2PartVoices& v, Vc2Keys& keys,
                 Vc2VoiceClassParts& parts);                                                               // +0x3c
    uint32_t changed(uint32_t voice, Vc2PartVoices& v, Vc2VoiceClassParts& parts);                         // +0x48

private:
    void keyWords(uint32_t voice, uint32_t message, Vc2PartVoices& v, Vc2Keys& keys);
    static Vc2SetCall setCall(uint32_t voice, uint32_t word);
};
