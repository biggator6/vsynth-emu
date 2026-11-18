#pragma once
// The analog converter's second part (v256): rom.4's 0x8939cf70 from its waveform switch (0x8939d3a2) to its end
// (0x8939da96), after the part it shares with the VOCODER's (vc2OscBlockConvert).  The block's analog words, rom.2's
// XtAnalogBlock's (XtAnalog.h) 0x30 up: the waveform's code, Fat or the sub oscillator's pitch with their depths, the
// Pulse Width and its depths, Impact, the Slope, the sub's level and LA's Tone.  Bit for bit: xt_vc2carrier's third part
// against rom.4's whole converter.  HARDWARE_FIRMWARE.md, "The VC-2's analog converter's second part".  Every address
// is rom.4's.
#include "Vc2OscConvert.h"
#include <cstdint>

// The OSC block's words from +0xb0
struct Vc2OscAnalogWords {
    int32_t code;           // +0xb0: the routine, by the waveform (0x8939d3b4's switch)
    float fat;              // +0xb4: Fat 0…1; with the sub oscillator on, its pitch in octaves
    float fatEnv;           // +0xb8: its envelope's depth × the velocity factor
    int32_t fatEnvWord;     // +0xbc: 3 on OSC 1, 7 on OSC 2
    float fatLfo;           // +0xc0: its LFO's depth
    int32_t wordC4;         // +0xc4: 0
    float width;            // +0xc8: the Pulse Width 0…1, wrapped (0x8939c5e0)
    float widthEnv;         // +0xcc: its envelope's depth × the velocity factor
    float widthLfo;         // +0xd0: its LFO's depth
    int32_t widthEnvWord;   // +0xd4: 2 on OSC 1, 6 on OSC 2
    int32_t wordD8;         // +0xd8: 0
    float impact;           // +0xdc: Impact × 4 ÷ 40
    float slope;            // +0xe0: SAW's and SQUARE's Slope ÷ 100 with Alias Cancel on (0x8939cae0), else 0
    float subOn;            // +0xe4: 1 with Sub Octave above 0
    float subLevel;         // +0xe8: the level curve at Sub Level
    float tone;             // +0xec: LA-SQUARE's Tone, the others' LA Saw Tone
    float toneKf;           // +0xf0: their Keyfollows
};
static_assert(sizeof(Vc2OscAnalogWords) == 0x44, "rom.4's layout, the OSC block's +0xb0…+0xf4");
inline constexpr int kVc2OscAnalogWordsAt = 0xb0;   // [FW] where they start in the block

// The part on the input and the world vc2OscBlockConvert was given
void vc2OscAnalogConvert(const uint8_t* input, const Vc2OscConvertIn& in, const Vc2ToneShared& shared,
                         Vc2OscAnalogWords& words);
