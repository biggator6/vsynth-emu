#pragma once
// The OSC block's converter (v251): what rom.4's OSC converters write into the OSC block before each kind's own part.
// The analog's converter (0x8939cf70, OSC Type 0 and the default of 0x8939fbd0's switch) to its waveform switch
// (0x8939d3b4), and the VOCODER's first half (0x8939db00…0x8939de28), are one code: the same words in the same order,
// the same helpers (0x8939c8a0, 0x8939c910, 0x8939c1a0, 0x8939c0e0, 0x8939c9a0), but for the calibration row the
// VOCODER gives 0x8939c9a0 (0) where the analog gives its waveform.  The note's pitch10 and the key follow's into the
// input; the block's key, cents, pitch ratio, envelope and LFO depths, level and gain.  Bit for bit: xt_vc2carrier's
// third part against rom.4's whole converters.  HARDWARE_FIRMWARE.md, "The VC-2's OSC block converter".  Every
// address is rom.4's.
#include "Vc2VocoderOsc.h"
#include <array>
#include <cstdint>

// A waveform's calibration, four signed bytes a row at rom.4's 0x89e371e0 + 0x1020e + 4·row, rom.2's OSC table
// +0xc20e from its hidden system IDs 0x528… (HARDWARE_FIRMWARE.md, "The analog calibration"; the same law in both)
struct Vc2OscCalibration {
    int8_t bias = 0;    // KF Bias Point, a key: pitch10 is measured from it × 1000
    int8_t upper = 0;   // Upper Level KF, above it
    int8_t lower = 0;   // Lower Level KF, below it
    int8_t level = 0;   // Level, the level curve's index (read signed)
};
inline constexpr int kVc2OscCalibrationRows = 11;   // [FW] 0x8939c9a0's switch: rows 0…10, past 10 row 0

// What the converter reads besides its input: the Matrix's groups for the OSC (its getters by the input's OSC); M's
// bytes +0x182 and +0x183, the pitch's (2·b183 − b182)/127 octaves, rom.2's Pitch ratio's law on its M's D-Beam
// controllers (+0x180…+0x183) at the same offsets; the pitch source's ratio (0x89394a40 on the tone generator's
// object 0x89989748, Vc2AutoNote::ratioFor); and the calibration
struct Vc2OscConvertIn {
    Vc2MatrixGroups sums {};
    std::array<uint8_t, 2> beam {};   // M + 0x182, M + 0x183
    float pitchSource = 1.0f;
    std::array<Vc2OscCalibration, kVc2OscCalibrationRows> calibration {};
};

// The block's words the part writes (the block the voice + 0x54, rom.4's layout from +0); `wordNN` a word it leaves
struct Vc2OscBlockHead {
    uint32_t kind;              // +0x00: the dispatch's (0x8939fb40), not the converter's
    uint8_t key;                // +0x04: the key Pitch KF gives, a note 0…127 (0x8939c910)
    uint8_t keyRest[3];
    float cents;                // +0x08: the note's pitch against A4, cents within ±6000
    float ratio;                // +0x0c: the Pitch knob's 2^(cents/1200), the M bytes' octaves, × the pitch source
    uint32_t words10[3];
    float pitchEnvDepth;        // +0x1c: cents (0x8939c1a0) × the velocity factor
    int32_t pitchEnvWord;       // +0x20: 1 on OSC 1, 5 on OSC 2
    uint32_t words24[4];
    float level;                // +0x34: the level curve at Level + the Matrix's sum, by its KF, × the velocity²
    int32_t levelEnvWord;       // +0x38: 4 on OSC 1, 8 on OSC 2
    float pitchLfoDepth;        // +0x3c: cents (0x8939c1a0) of the depth with the LFO group's Pitch sum
    int32_t word40;             // +0x40: 0
    uint32_t words44[4];
    float levelLfoDepth;        // +0x54: (d/63)·|d/63| of the depth with the LFO group's Level sum
    int32_t word58;             // +0x58: 0
    float gain;                 // +0x5c: 2^(Wave Gain/6) × the calibration's gain (0x8939c9a0)
    uint32_t words60[2];
    float keyCents;             // +0x68: the key follow's pitch10 (the input's +0x98) against A4, cents within ±6000
};
static_assert(sizeof(Vc2OscBlockHead) == 0x6c, "rom.4's layout, the OSC block's +0x00…+0x6c");

// The part on a converter's input (the note-on's, 0x8939fe08: an 8-byte header, the OSC's record from +8, the note's
// coarse +0x88, fine +0x90 and second key +0x9c, M at +0x100), into the block's head; `row` the calibration row:
// the analog's waveform (the input's +0x28), the VOCODER's 0.  Writes the input's +0x94 and +0x98 as rom.4 does.
void vc2OscBlockConvert(uint8_t* input, int row, const Vc2OscConvertIn& in, const Vc2ToneShared& shared,
                        Vc2OscBlockHead& block);
// The analog's (0x8939cf70) and the VOCODER's (0x8939db00) calibration rows: the input's waveform byte, and 0
int vc2OscAnalogRow(const uint8_t* input);
inline constexpr int kVc2OscVocoderRow = 0;   // [FW] 0x8939dce4: mov #0,r4
