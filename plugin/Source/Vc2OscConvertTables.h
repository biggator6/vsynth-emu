#pragma once
// The OSC block's converter's constants from rom.4 (v251): the analog converter's first part's pool words (0x8939cf70…,
// whose VOCODER copy holds the same), its helpers' (the cents curve 0x8939c1a0, the calibration's gain 0x8939c9a0),
// the cents curve's bytes and the level curve's words, read once by loadVc2 at the start (Vc2RomTables), never after.
// HARDWARE_FIRMWARE.md, "The VC-2's OSC block converter".
#include <array>
#include <cstdint>

// The cents curve's lines past its table: up to `top`, x × slope + offset (0x8939c1a0)
struct Vc2OscCentsLine {
    float top = 0, offset = 0, slope = 0;
};

struct Vc2OscConvertTables {
    float perTenth = 0;                  // 0x8939d07c: 10, the cents' tenths
    float centsMost = 0, centsLeast = 0;  // 0x8939d084, 0x8939d310: ±6000
    float depthMost = 0, depthLeast = 0;  // 0x8939d31c, 0x8939d324: ±63, the Pitch knob with the Matrix's sum
    float sumScale = 0;                  // 0x8939d320: 12000, the Matrix's sum onto the knob, × 63 ÷ it
    float perOctave = 0;                 // 0x8939d328: 1200, the cents into powf's octaves
    float beamScale = 0;                 // 0x8939d330: 127, M's two bytes into octaves
    float velocityScale = 0;             // 0x8939d340: 32768, a velocity factor
    float gainScale = 0;                 // 0x8939d350: 6, Wave Gain's dB into powf's octaves
    // The cents curve: its table's last index (12), then its lines to 36, 48, 60 and past
    float tableTop = 0;                  // 0x8939c2dc
    std::array<Vc2OscCentsLine, 4> lines {};   // 0x8939c2e0…0x8939c308; the last one's top unread (0)
    float kfScale = 0;                   // 0x8939cb24: 6e6, a calibration keyfollow's step
    float levelScale = 0;                // 0x8939cb2c: 32768, the level curve's word
    std::array<uint8_t, 13> cents {};    // 0x89540114: the cents curve's table, 0…12
    // 0x89541f34: the level curve, by an index −128…+127 (the calibration's Level byte is read signed: below 0 the
    // words before it)
    std::array<uint16_t, 256> level {};
};

[[nodiscard]] bool vc2LoadOscConvert(const char* path);
bool vc2OscConvertLoaded();
const Vc2OscConvertTables& vc2OscConvertTables();
// The level curve's word at an index −128…+127
inline uint16_t vc2OscLevelWord(const Vc2OscConvertTables& t, int index) { return t.level[(size_t)(index + 128)]; }
