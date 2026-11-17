#pragma once
// The VC-2's COSM type 8, the GROWL (v234), which takes the V-Synth's Comb Filter's place and block code (9) in rom.4.
// Two taps 512 back on 3267-sample lines, one moved in level and one in position by a phase at a subharmonic of
// 440 × the pitch ratio, 5…110 Hz by Speed, its depth Intencity; the type's other five parameters do nothing.  rom.4's
// own routines: the converter 0x89398780, the note-on's case 0x893b9d30, the update's 0x893b9d40, the new block's
// 0x893b9dc0 (with the COSM clamp 0x893bf240) and the DSP 0x893b9b40, mono and stereo alike.  What the COSM object's
// frame does around them (the note-on's clears, the lines' allocation, the levels it holds) is the frame's, not here.
// Bit for bit: xt_vc2cosm against rom.4's routines on an object in Sh4Interp, every word after each.
// HARDWARE_FIRMWARE.md, "The VC-2's COSM type 8, the Growl"; research/VC2_VOCAL_DESIGNER.md, "COSM type 8, the Growl,
// run".  Audio thread.
#include "Vc2CosmConvert.h"
#include "Vc2CosmInput.h"
#include "Vc2CosmTables.h"
#include <cstdint>
#include <vector>

// The Growl's block as its converter writes it: the first 0x2c of a COSM's 0x48 bytes, the rest left as they were.
// The envelopes' and LFOs' numbers are the tone's binding's (0x893b4660: type 9 reads +0x10, +0x20, +0x18, +0x28)
struct Vc2CosmGrowlBlock {
    int32_t code;                   // +0x00: 9
    float intensity, speed;         // +0x04: Intencity I/127; +0x08: Speed 2·S/128 − 1
    Vc2CosmMod intensityMod;        // +0x0c…+0x1b: Intencity's depths and numbers, all 0
    Vc2CosmMod speedMod;            // +0x1c…+0x2b: Speed's, all 0
};
static_assert(sizeof(Vc2CosmGrowlBlock) == 0x2c, "rom.4's layout, the Growl's block");

// rom.4's COSM object (0x164 bytes, one for every type, two to a tone at its +0x1728 and +0x188c), word for word: the
// words the Growl's routines read and write named, every other kept as it was.  The DSP's state D is the object + 0x58.
struct Vc2CosmGrowlWords {
    float pitch;                    // +0x000: the voice's pitch ratio against its wave's root (the frame's update)
    uint32_t unread004[3];          // +0x004…+0x00f
    Vc2CosmGrowlBlock block;        // +0x010…+0x03b: the block, its code from the note-on, the rest from a new block
    uint32_t unread03c[7];          // +0x03c…+0x057
    uint32_t dsp;                   // +0x058: D+0, the code's byte (the frame's)
    int32_t position;               // +0x05c: D+4, where the lines were last written, 0…3266
    uint32_t left, buffer2, right, buffer4;   // +0x060…+0x06c: D+8…D+0x14, the frame's four buffers: the lines L, R
    uint32_t unread070;             // +0x070
    float phase, step;              // +0x074, +0x078: D+0x1c, D+0x20, the growl's, −1…1; [FW] single, as rom.4 keeps it
    uint32_t unread07c[32];         // +0x07c…+0x0fb
    float depth, factor;            // +0x0fc, +0x100: D+0xa4, D+0xa8, the update's depth and divider factor
    float envA, envB, lfoA, lfoB;   // +0x104…+0x110: the levels the frame holds from the note-on or the last update
    uint32_t unread114[20];         // +0x114…+0x163
};
static_assert(sizeof(Vc2CosmGrowlWords) == 0x164, "rom.4's layout, the COSM object");

class Vc2CosmGrowl {
public:
    static constexpr int32_t kCode = 9;                                     // [FW] its block code, the Comb's in rom.2
    static constexpr int kLine = 3267, kTapBack = 512, kFrames = 32;        // [FW] the lines, the tap, a block's frames

    Vc2CosmGrowl();
    void noteOn();                              // 0x893b9d30: the phase and its step 1
    void update();                              // 0x893b9d40: depth and divider factor, from the block and the levels
    void newBlock(const Vc2CosmGrowlBlock& b);  // 0x893b9dc0: the block's words from +4 into the object's, then update
    void render(float* frames);                 // 0x893b9b40: kFrames L/R frames in place

    Vc2CosmGrowlWords w {};
    std::vector<float> left, right;             // the lines, kLine each: the frame gives and clears them

private:
    float line(const std::vector<float>& l, int32_t i) const;
};

void vc2GrowlConvert(const Vc2CosmInput& in, Vc2CosmGrowlBlock& b, Vc2CosmMatrix& matrix);   // 0x89398780
float vc2CosmClamp(float x, float lo, float hi);   // 0x893bf240: lo first, then hi (a NaN passes)
