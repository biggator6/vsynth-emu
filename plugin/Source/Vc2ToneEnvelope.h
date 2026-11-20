#pragma once
// The VC-2 tone's envelope (v268): rom.4's object, 0x70 bytes word for word, over XtEnvelope.  rom.4's envelope code
// (0x893ab5b0…0x893ac4a0) is rom.2's opcode for opcode but for its pool's addresses, so its laws are XtEnvelope's, run on
// rom.4's words and stored back.  Its converter (0x8939a400) writes curve 0 or 2, scale 1 and sync 0 alone: curve 1's
// output (0x893abcb0) and the synced rate at +0x10 are never reached, and neither is ported.  The settings and release
// from v315.  Bit for bit: xt_vc2tone against rom.4's own routines.  HARDWARE_FIRMWARE.md, "The VC-2's envelopes and
// LFOs", "The VC-2's envelopes' and LFOs' settings and release".  Audio thread.
#include <array>
#include <cstdint>

class XtEnvelope;

struct Vc2EnvelopeWords {
    int32_t stage = 0;               // +0x00: −1 built, 0 attack … 3 sustain, 4 release, 5 done
    int32_t index = 0;               // +0x04: the tone's number for it (Vc2Tone::envelopeIndex holds it)
    int32_t curve = 0;               // +0x08: the block's words, +0x08…+0x3f
    float scale = 0.0f;              // +0x0c
    uint32_t sync = 0;               // +0x10
    std::array<uint32_t, 2> clock {};   // +0x14, +0x18: the converter's 0x124f80, which nothing here reads
    std::array<float, 4> time {};    // +0x1c: attack, none, decay, release, ms
    std::array<float, 5> level {};   // +0x2c: start, peak, peak, sustain, end
    int32_t rate = 0;                // +0x40
    int32_t phase = 0;               // +0x44
    float unread = 0.0f;             // +0x48: 1 from the constructor, read by nothing here
    float out = 0.0f;                // +0x4c
    float release = 0.0f;            // +0x50: the level a release starts from, which only the release writes
    float coef = 0.0f;               // +0x54
    float onset = 0.0f;              // +0x58
    std::array<uint32_t, 3> output {};   // +0x5c: the output routine's member pointer, by the curve
    uint32_t on = 0;                 // +0x68: the tone ticks it when set
    float stageTime = 0.0f;          // +0x6c
};
static_assert(sizeof(Vc2EnvelopeWords) == 0x70, "rom.4's layout, an envelope's 0x70 bytes");

class Vc2ToneEnvelope {
public:
    void construct();                     // 0x893abe40 on an envelope given: what it leaves alone keeps the last use's
    void noteOn(const uint32_t* block);   // 0x893ac1c0: the converter's 14 words
    void tick(int32_t n);                 // 0x893abe80
    // 0x893ac110 (v315): a new block's times, levels and scale while the note sounds, XtEnvelope's update (rom.2's
    // 0x894f0080): a sustain moves to its new level at once
    void settings(const uint32_t* block);
    void release();                       // 0x893ac470 (v315): a key let go, XtEnvelope's note-off (0x894f03e0)
    bool started() const { return w.output[2] != 0; }   // a note-on has given it an output routine

    Vc2EnvelopeWords w {};

private:
    void load(XtEnvelope& e) const;
    void store(const XtEnvelope& e);
};
