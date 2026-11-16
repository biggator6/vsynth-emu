#pragma once
// The VC-2's vocoder object (v220): rom.4's static object (0x89999758, 0x5a78 bytes), its two engines (HIRESO at
// +0x024, LEGACY at +0x2d94) and the input and output stages around them (Vc2Unvoiced, rom.4's globals).  The builder
// 0x893f0490; the mode switch 0x893f0600, which chooses the engine once and HIRESO's input 128 samples back in the
// mic ring; the settings path 0x893f06a0, with the edit page's words; the block drivers 0x893f0510 (mono) and
// 0x893f0580 (stereo); the pedal 0x893f0770; the glue 0x893f07a0, each block.  Bit for bit: xt_vc2vocoder against
// rom.4's own object in Sh4Interp, every part of it after each event and block.  HARDWARE_FIRMWARE.md, "The vocoder
// object", "The vocoder object, read".  Audio thread, but for the constructor (the stages' buffers).
#include "Vc2Hireso.h"
#include "Vc2Legacy.h"
#include "Vc2Unvoiced.h"
#include "Vc2VocoderTables.h"

// The object's words before its engines, as rom.4 lays them out from +0x000 (its +0x020, the playing engine's address,
// is `Vc2Vocoder::engine`)
struct Vc2VocoderWords {
    int32_t mode;        // +0x00: 0 off, 1 LEGACY, 2 HIRESO
    float ratio[2];      // +0x04: each side's Formant Shift ratio, the glue's to the engine's slot 4
    float offset[2];     // +0x0c: each side's ratio offset by the tracked pitch, 0 for none
    float gain;          // +0x14: the stages' consonants' gain, gliding to target × the glue's scale
    float target;        // +0x18: Unvoice Level × Vocoder Depth × hiresoOn
    int32_t hiresoOn;    // +0x1c: 1 while HIRESO plays, else 0 (LEGACY has consonants of its own)
};
static_assert(sizeof(Vc2VocoderWords) == 0x20, "rom.4's layout, +0x000…+0x020");

class Vc2Vocoder {
public:
    static constexpr int32_t kOff = 0, kLegacy = 1, kHireso = 2;   // the modes, rom.4's (Vocoder Type + 1) × Sw
    static constexpr int kHiresoLatency = 128;   // [FW] HIRESO's hook: its input this many samples back in the mic ring

    Vc2Vocoder();                                // 0x893f0490: both engines built, the mode off
    void setMode(int mode);                      // 0x893f0600: the same mode does nothing; another, the engine's slot 0
    void settings(Vc2VocoderSettings& in);       // 0x893f06a0, which writes the pivots (and the engine's slot 5 may)
    // 0x893f0510 (one channel) and 0x893f0580 (two): `in` the mic's block (the stage's micBlock(), where the part mixer
    // writes it), the frames in place, levelMod the output stage's word (0x89997298); with no engine, nothing
    void block(const float* in, float* frames, int channels, float levelMod);
    void hold(int controller);                   // 0x893f0770: the engine's slot 6
    // 0x893f07a0, each block before it: the gain glides to target × scale, and each side's ratio goes to the engine's
    // slot 4, moved by the tracked pitch where an offset is set (`period`, the tracker's, 0x893ee010's)
    void glue(float scale, float period);
    void editing(int on) { edit.on = on; }       // 0x893f0960
    void editSide(int pivot, int side, float ratio, float offset);   // 0x893f0970: a side's three edit words

    Vc2VocoderWords w {};
    Vc2Hireso hireso;
    Vc2Legacy legacy;
    Vc2Unvoiced stage;                           // the input stage (0x893ed040) and the output stage (0x893ed510, 0x893ed690)
    Vc2VocoderEdit edit;                         // the edit page's words, from rom.4's .data at the start
    Vc2Engine* engine = nullptr;                 // +0x020: the playing engine, none when off

private:
    // The hook (rom.4's word at 0x899d9d8c): the engine's input from the block's; HIRESO's from the mic ring
    const float* (Vc2Vocoder::*hook)(const float*) const = &Vc2Vocoder::asGiven;
    const float* asGiven(const float* in) const { return in; }   // LEGACY's, 0x893f0480, a bare rts
    const float* latencyBack(const float*) const;                // HIRESO's, 0x893ed420
};
