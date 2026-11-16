#pragma once
// The VC-2's vocoder engines as the vocoder object calls them (v219): rom.4's engine base class (0x89506f24, its first
// five methods pure virtual) by its vtable's eight slots, which LEGACY (Vc2Legacy) and HIRESO (Vc2Hireso) implement,
// and the settings struct the vocoder's settings path (0x893f06a0) passes to slot 5.  Each engine's slots are checked
// against its own in xt_vc2legacy and xt_vc2hireso; HARDWARE_FIRMWARE.md, "The vocoder object"; research/
// VC2_VOCAL_DESIGNER.md, "The vocoder object".  Audio thread.
#include <cstdint>

// The vocoder's settings, as the converter builds them and rom.4 lays them out (its +0x00…+0x3c): the vocoder's
// settings path reads them all, an engine's slot 5 its own (LEGACY +0x00 and +0x38; HIRESO +0x10, +0x14, +0x28…+0x38)
struct Vc2VocoderSettings {
    float ratio[2];             // +0x00: each side's Formant Shift ratio
    float offset[2];            // +0x08: each side's ratio offset by the tracked pitch, 0 for none
    int32_t pivot[2];           // +0x10: each side's pivot, or below 0 for its template's (HIRESO's slot 5 writes it here)
    float soundLevel;           // +0x18: Vocoder Sound Level ÷ 127
    float unvoiceLevel;         // +0x1c: Unvoice Level ÷ 127
    int32_t spread;             // +0x20: Unvoice Spread
    int32_t detect;             // +0x24: Unvoice Detect + 24
    int32_t attack, release;    // +0x28: Env Attack, Env Release, 0…100 (101 on: left as they stand)
    int32_t formantTemplate;    // +0x30: 0…15 two records, 16 on the RAM curves, below 0 left as it stands
    float depth;                // +0x34: Vocoder Depth ÷ 127
    float dump;                 // +0x38: Hold Dump Time ÷ 127
};
static_assert(sizeof(Vc2VocoderSettings) == 0x3c, "rom.4's settings struct, +0x00…+0x3c");

// An engine by its slots, as the vocoder's block drivers, settings path, glue and pedal call them
class Vc2Engine {
public:
    virtual ~Vc2Engine() = default;
    virtual void reset() = 0;                                // slot 0, at a mode change
    virtual void analyze(const float* in, bool voiced) = 0;  // slot 1: the mic's block; voiced the input stage's query
    virtual void mono(float* frames) = 0;                    // slot 2: 32 frames, the carrier in each first word
    virtual void stereo(float* frames) = 0;                  // slot 3: 32 frames, a carrier in each word
    virtual void formant(int side, float ratio) = 0;         // slot 4: a side's Formant Shift ratio
    virtual void settings(Vc2VocoderSettings& in) = 0;       // slot 5
    virtual void hold(int controller) = 0;                   // slot 6: the Hold pedal's 0…127
    virtual float holdLevel() const = 0;                     // slot 7: 1 less the hold's share
};
