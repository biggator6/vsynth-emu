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

class XtExtInput;

// Where the outside input comes from (v207): asked at each block's output (XtSynth::busOut) for the block's 32 frames of
// it, which go into the chip's input slots with the Effects on, and with them off back as the chip would hand them
// (XtExtInCopy).  Either way they reach the EXT IN OSCs the next block.  The plugin's is XtHostInput.  Audio thread.
class XtExtInFeed {
public:
    virtual ~XtExtInFeed() = default;
    // This block's 32 frames, int32 L then R at 44.1 kHz and the XT's full scale (±2^31), until the next call
    virtual const int32_t* block() = 0;
    // Those frames, `lr`, as program A would hand them back, into what the EXT IN OSCs play next
    virtual void withoutChip(XtExtInput& to, const int32_t* lr) = 0;
};

// What the EXT IN players read: the input as the effect chip returns it, int32 L then R a frame at 44.1 kHz, pushed as it
// comes and taken 32 frames a block.  A block short of frames is zeros past them.  The audio thread's, as XtSynth is.
class XtExtInput {
public:
    static constexpr uint32_t kFrames = 4096;   // the most it holds, a power of two: a push past it keeps the newest
    void push(const int32_t* lr, int frames);
    const int32_t* block();   // the next 32 frames, 64 words, until the next call
    const int32_t* take(int frames);   // the next `frames` of them, at most 32, the same way (XtFxUnit's, a frame at a time)
    void clear();
    uint32_t held() const { return write_ - read_; }   // the frames pushed and not yet taken
    uint32_t taken() const { return read_; }           // v455: the frames taken, or passed over by an overrun, counted on
    // Where the frames come from, if anywhere: XtSynth::busOut asks it each block (v207).  Set before the audio runs.
    void setFeed(XtExtInFeed* f) { feed_ = f; }
    XtExtInFeed* feed() const { return feed_; }

private:
    std::array<int32_t, 2 * kFrames> ring_ {};
    std::array<int32_t, 64> out_ {};
    uint32_t read_ = 0, write_ = 0;             // frames, counted on; their difference is what's held
    XtExtInFeed* feed_ = nullptr;
};

// The outside input as the effect chip's program A hands it back to the SH-4, computed without the chip (v205, the user's
// choice for the Effects switch off): words 4 and 5 by External Input Type's law, a frame late, in the chip's 24 bits.
// Settled, the chip's to the bit (xt_fx control, "the copy without the chip"; HARDWARE_FIRMWARE.md, "The input with the
// Effects off").  A change of Type takes at once here, where the chip's routing goes through the driver's queue and fades.
// Source makes no difference to the plugin's one input, which the unit gives both pairs of the chip's slots.  Audio thread.
class XtExtInCopy {
public:
    void setType(int type);   // External Input Type, ID 0x78: 0 STEREO L R, 1 MONO MIX, 2 MONO L, 3 MONO R
    // `frames` frames of the input, int32 L then R at the XT's full scale, into `to` as the chip would hand them back
    void into(XtExtInput& to, const int32_t* lr, int frames);

private:
    int32_t wordOf(int side) const;
    int type_ = 1;            // the descriptor's default, MONO MIX
    int32_t last_[2] = {};    // the frame before, in the chip's 24 bits: what goes back now
};

#ifdef XT_TONE_CHECK
// xt_tone's input, the same frames to its firmware's tone and to XtSynth: the 32 from frame `from` (the block's first), each
// word drawn from the frame and side alone, over the whole of int32
void xtExtInTestBlock(long from, int32_t* lr64);
#endif
