#pragma once
// The EXT IN OSC (v203): OSC Type 2, a voice of kind 4.  Its player (0x895097d0) reads the input as the effect chip
// returns it to the SH-4: words 4 and 5 of each 32-byte frame of the block the DMA brought back ([0x8987c7a0], that block
// + 16, set at 0x894daca2), at the OSC's gain, which glides to its target by 1/32 a sample.  XtExtInput holds that input
// for XtSynth.  HARDWARE_FIRMWARE.md, "The EXT IN OSC".
#include <array>
#include <cstdint>

class XtExtInPlayer {
public:
    // At note-on (0x894f2ff0): the gain from its start (+0x158, 0x894f3e20), going to its target (+0x154, 0x894f3dc0)
    void noteOn(float start, float target);
    // At each update (0x89509780): the target as it stands, with no step toward it
    void setTarget(float target) { target_ = target; }
    float gainNow() const { return gain_; }
    // 32 frames into lr (L, R), the input's 32 at the gain as it goes, in the XT's units (int16 × 65536)
    void block(float* lr, const int32_t* in);

private:
    float gain_ = 0.0f, target_ = 0.0f;   // the player's block +0x84 and +0x80: the voice's +0x158 and +0x154
};

// What the EXT IN players read: the input as the effect chip returns it, int32 L then R a frame at 44.1 kHz, pushed as it
// comes and taken 32 frames a block.  A block short of frames is zeros past them.  The audio thread's, as XtSynth is.
class XtExtInput {
public:
    void push(const int32_t* lr, int frames);
    const int32_t* block();   // the next 32 frames, 64 words, until the next call
    void clear();

private:
    static constexpr uint32_t kFrames = 4096;   // the ring's frames, a power of two
    std::array<int32_t, 2 * kFrames> ring_ {};
    std::array<int32_t, 64> out_ {};
    uint32_t read_ = 0, write_ = 0;             // frames, counted on; their difference is what's held
};

#ifdef XT_TONE_CHECK
// xt_tone's input, the same frames to its firmware's tone and to XtSynth: the 32 from frame `from` (the block's first), each
// word drawn from the frame and side alone, over the whole of int32
void xtExtInTestBlock(long from, int32_t* lr64);
#endif
