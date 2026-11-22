#pragma once
// The VC-2 tone's LFO (v268): rom.4's object, 0x98 bytes word for word, over XtLfo.  rom.4's LFO code (0x893ac4f0…
// 0x893ad720) is rom.2's opcode for opcode but for its pool's addresses, so its laws are XtLfo's, run on rom.4's words
// and stored back.  The note-on is rom.4's own order on the words as they stand, where XtLfo::noteOn starts from a base
// just built.  A key-unsynced note starts from another tone's LFO or a free object (v271, Vc2FreeLfos).  The settings
// and release from v315.  Bit for bit: xt_vc2tone against rom.4's own routines.  HARDWARE_FIRMWARE.md, "The VC-2's
// envelopes and LFOs", "The VC-2's free LFOs", "The VC-2's envelopes' and LFOs' settings and release".  Audio thread.
#include <cstdint>

class XtLfo;
class Vc2FreeLfos;

struct Vc2LfoWords {
    uint8_t state = 0;        // +0x00: the delay and fade's
    int8_t direction = 0;     // +0x01
    int16_t count = 0;        // +0x02: the delay's ms so far
    float fadeIn = 0.0f;      // +0x04
    float fadeOut = 0.0f;     // +0x08
    float delay = 0.0f;       // +0x0c
    float rateIn = 0.0f;      // +0x10
    float rateOut = 0.0f;     // +0x14
    float rest = 0.0f;        // +0x18
    float target = 0.0f;      // +0x1c
    float amplitude = 0.0f;   // +0x20
    uint32_t vtable = 0;      // +0x24
    uint32_t active = 0;      // +0x28
    uint32_t mode = 0;        // +0x2c: the fade mode the note-on took
    float fadeLevel = 0.0f;   // +0x30: the amplitude over its full scale, which each tick leaves (0x893ac8a0)
    int32_t index = 0;        // +0x34: the tone's number for it (Vc2Tone::lfoIndex holds it)
    float out = 0.0f;         // +0x38
    float unread3c = 0.0f;    // +0x3c: 0 from the constructor
    float phase = 0.0f;       // [FW] +0x40 (0x893ad100): single precision, as rom.4 keeps it
    float previous = 0.0f;    // +0x44
    float held = 0.0f;        // +0x48
    int32_t wave = 0;         // +0x4c: the block's words, +0x4c…+0x6b
    float rate = 0.0f;        // +0x50
    float delayMs = 0.0f;     // +0x54
    float fadeMs = 0.0f;      // +0x58
    int32_t keySync = 0;      // +0x5c
    int32_t sync = 0;         // +0x60
    int32_t offset = 0;       // +0x64
    int32_t fadeMode = 0;     // +0x68
    float chaos = 0.0f;       // +0x6c
    uint32_t unread70 = 0;    // +0x70
    float tempo = 0.0f;       // +0x74
    uint32_t waveCase = 0;    // +0x78: 0 for the eight waves a converter gives
    uint32_t follow = 0;      // +0x7c: the free object it follows
    uint32_t on = 0;          // +0x80: the tone ticks it when set
    float clockStep = 0.0f;   // +0x84
    float clockTarget = 0.0f; // +0x88
    float clockTime = 0.0f;   // +0x8c
    int32_t clockWrap = 0;    // +0x90
    int32_t clockCount = 0;   // +0x94
};
static_assert(sizeof(Vc2LfoWords) == 0x98, "rom.4's layout, an LFO's 0x98 bytes");

class Vc2ToneLfo {
public:
    static constexpr float kChaosStart = 0.7f;   // [FW] the generator's word 0x89999668, as 0x893ad8d0 sets it

    void construct(float& generator);    // 0x893acbf0: its chaos state the shared generator's next value, made positive
    void destroy();                      // 0x893acc70 but the delete
    void noteOn(const uint32_t* block, float tempo);   // 0x893acda0
    // 0x893ace60: key-unsynced with another tone of the id sounding: that tone's LFO's state, and what it follows
    void follow(const Vc2ToneLfo& other, const uint32_t* block, Vc2FreeLfos& frees);
    void attach(uint32_t free, Vc2FreeLfos& frees);    // 0x893acfc0: it follows a free object
    // 0x893acf10: key-unsynced with none sounding: the free object takes this note's rate, the LFO its phase and clock
    void startFree(const uint32_t* block, float tempo, Vc2FreeLfos& frees);
    void tick(int32_t n);                // 0x893ad100
    void clock(float seconds);           // 0x893ad080
    // 0x893acff0 (v315): a new block while the note sounds: its wave, rate, delay, fade, offset and fade mode, not its
    // two syncs; the fade's times again in the direction the note-on chose, and a note value's step
    void settings(const uint32_t* block);
    // 0x893ac8c0 (v315): a key let go: the fade starts for the two modes that start it there, by the mode the note-on took
    void release();

    Vc2LfoWords w {};

private:
    void takeBlock(const uint32_t* block);
    void trigger();                      // 0x893acca0: the fade by the block's mode, then a tick of none
    void load(XtLfo& l) const;
    void store(const XtLfo& l);
    void storeTicked(const XtLfo& l);    // store, with the word only a tick writes
};
