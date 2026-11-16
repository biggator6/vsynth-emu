#pragma once
// The VC-2's TVA (v233): rom.4's TVA object, the tone's +0x19f0, 0xc8 bytes, its words word for word: the block a
// note-on or a new block copies in (+0x00…+0x50), the truncation words, the fade, two steps, targets and gains, eight
// of the block's words copied at the note-on, the samples since the note-on or the last update, the bindings (its
// envelope and two LFOs by rom.4's addresses, the tone's note-on's) and their products.  The constructor 0x893b20b0,
// the note-on 0x893b2100, the new block 0x893b23b0, the update 0x893b23d0, the end tests 0x893b2560 and 0x893b2610,
// the two truncations 0x893b25a0 and 0x893b25b0, silent 0x893b25c0, the render 0x893b2640 (two gains onto one stereo
// pair, its shortened step kept; truncated, a fade) and the envelope's level 0x893b2780.  What it reads of its
// envelope and LFOs, the fade's step and the OS's call through Vc2TvaParts.  Bit for bit: xt_vc2tva against rom.4's
// routines in Sh4Interp.  HARDWARE_FIRMWARE.md, "The VC-2's TVA".  Audio thread.
#include <array>
#include <bit>
#include <cstdint>

// What the TVA reads of the envelope its +0xa4 binds, by the envelope's offsets (XtEnvelope's names)
struct Vc2TvaEnvelope {
    int32_t state = 0;     // +0x00: 5 done
    float attack = 0;      // +0x1c: the attack's time, ms
    float next = 0;        // +0x20: the next stage's, ms, the note-on's ramp when the attack's is 0
    float out = 0;         // +0x4c: its output
    float onset = 0;       // +0x58: the level a note's gain starts from
    float stageTime = 0;   // +0x6c: the stage's time, ms, below 0 in sustain
};

// What the TVA reads of its parts and what it calls beyond itself; a part by its rom.4 address
class Vc2TvaParts {
public:
    virtual ~Vc2TvaParts() = default;
    virtual Vc2TvaEnvelope envelope(uint32_t at) = 0;
    virtual float lfoOut(uint32_t at) = 0;   // an LFO's +0x38
    virtual float fadeStep() = 0;            // the truncated fade's step a sample, rom.4's word at 0x8993eb08
    virtual void setEvent(uint32_t event, uint32_t flags) = 0;   // 0x89301830, the OS's
};

// The object, as rom.4 lays it out
struct Vc2TvaWords {
    std::array<uint32_t, 0x14> block {};   // +0x00…+0x50: the note-on's or a new block's, copied
    uint32_t truncatedTold = 0;   // +0x50: 0x893b25a0's: the fade's end tells the OS
    uint32_t stopped = 0;         // +0x54: set at the fade's end when +0x58 is
    uint32_t truncated = 0;       // +0x58: 0x893b25b0's
    uint32_t oscFlag = 0;         // +0x5c: OSC 1's +0xa40 at the note-on
    float one = 0;                // +0x60: 1 from the constructor; nothing here reads it
    float fade = 0;               // +0x64: the truncated fade, from 1 to the floor
    std::array<float, 2> step {};     // +0x68, +0x6c: the gains' steps a sample, left and right
    std::array<float, 2> target {};   // +0x70, +0x74
    std::array<float, 2> gain {};     // +0x78, +0x7c
    std::array<uint32_t, 8> levels {};   // +0x80…+0x9c: the block's +0x20…+0x3c at the note-on (rom.2's bus levels)
    float elapsed = 0;            // +0xa0: samples since the note-on or the last update, 32 a render
    uint32_t envelope = 0, lfo = 0, panLfo = 0;   // +0xa4, +0xa8, +0xac: the bindings, rom.4's addresses
    float levelEnv = 0;           // +0xb0: the block's level × the envelope's output
    uint32_t unread = 0;          // +0xb4: nothing here touches it
    float depth = 0;              // +0xb8: the LFO's output × the block's depth
    float pan = 0;                // +0xbc: the pan LFO's output × the block's pan depth
    uint32_t serial = 0;          // +0xc0: the tone's serial (the tone's note-on)
    float zero = 0;               // +0xc4: 0 from the constructor; nothing here reads it
};
static_assert(sizeof(Vc2TvaWords) == 0xc8, "rom.4's layout, the TVA's 0xc8 bytes");

class Vc2Tva {
public:
    static constexpr int kFrames = 32, kOut = 2 * kFrames;   // a render's: 32 frames of a stereo pair
    static constexpr int32_t kDone = 5;                       // [FW] the envelope's state when it's done

    void construct();                                                            // 0x893b20b0 on a TVA given
    void noteOn(const uint32_t* block, uint32_t oscFlag, Vc2TvaParts& parts);   // 0x893b2100
    void newBlock(const uint32_t* block);         // 0x893b23b0 (its call, 0x893b1ea0, returns at once)
    void update(Vc2TvaParts& parts);              // 0x893b23d0
    bool over(Vc2TvaParts& parts) const;          // 0x893b2560: its envelope done and both gains at the floor
    uint32_t stoppedWord() const;                 // 0x893b2610
    void truncateTold();                          // 0x893b25a0
    void truncate();                              // 0x893b25b0
    // 0x893b25c0, which the tone's held() asks (Vc2Tone.h's tvaHeld): truncated, its fade at the floor; else over()
    // without its first test
    bool silent(Vc2TvaParts& parts) const;
    void render(const float* in, float* out, Vc2TvaParts& parts);   // 0x893b2640: kFrames frames into out, added
    float envelopeLevel(Vc2TvaParts& parts) const;   // 0x893b2780: its envelope's output

    // The block's floats by their offsets in it
    float blockF(uint32_t at) const { return std::bit_cast<float>(w.block[at / 4]); }

    Vc2TvaWords w {};

private:
    Vc2TvaEnvelope products(Vc2TvaParts& parts);  // the envelope read, and its and the LFOs' products formed
    void aim(float level, float* to) const;       // two targets or gains from a level, by the LFOs' products
    void stepsOver(float samples);                // each gain's step a sample to its target
    bool gainsDown() const;                       // both gains at or below the floor
    void fadeOut(const float* in, float* out, Vc2TvaParts& parts);   // the render's truncated path
};
