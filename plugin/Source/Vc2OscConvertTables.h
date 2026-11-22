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
    // The analog converter's second part's (v256)
    float widthSteps = 0;                // 0x8939c814: 126, the Pulse Width's wrap
    float detuneKeyScale = 0;            // 0x8939c820: 1/120, the Detune's keyfollow
    float sumLeast = 0;                  // 0x8939c800: −12000, the Detune with the Matrix's sum
    float subLowest = 0;                 // 0x8939d890: −2, Sub Octave's first step, in octaves
    float impactScale = 0, impactPer = 0;   // 0x8939d634, 0x8939d638: 4 and 40, Impact × 4 ÷ 40
    float slopeScale = 0;                // 0x8939cd7c: 100, a Slope's word
    // The Detune curve's lines, cents, to 10, 15, 31, 51 and past (0x8939c824…0x8939c84c; its first line's slope and
    // offset and the second's slope are immediates: 1, 0 and 2); the last one's top unread (0)
    std::array<Vc2OscCentsLine, 5> detune {};
    std::array<uint8_t, 13> cents {};    // 0x89540114: the cents curve's table, 0…12
    // 0x89541f34: the level curve, by an index −128…+255 (the calibration's Level byte is read signed: below 0 the
    // words before it; the Sub Level byte unsigned: past 127 the words after it)
    std::array<uint16_t, 384> level {};
    // EXTIN's converter's (v393): its gain's divisor (0x8939edf8, 6), its velocity factor's (0x8939edec, 32768) and its
    // depths' (0x8939edc4, 63); the formant law 0x8939c490's 0.2 (0x8939c59c), 1200 (0x8939c560), 12000 (0x8939c56c)
    // and 63 (0x8939c568), its low hold `sumLeast`'s, and its rows: 11 of four pairs of a key and an offset, from rom.4's
    // .data at 0x8993d0c8
    float extinGainScale = 0, extinVelocityScale = 0, extinDepthTop = 0;
    float formantStep = 0, formantCents = 0, formantMost = 0, formantDepth = 0;
    std::array<std::array<int32_t, 8>, 11> formantRows {};
    // PCM's converter's (v394): its depths' 63 (0x8939e4b4), velocity scale (0x8939e4dc, 32768) and gain divisor
    // (0x8939e8d8, 6); the envelope and LFO Time depths' curve, its lines' knee 60 (0x8939e724), −120 (0x8939e728), 3
    // (0x8939e72c), −60 (0x8939e730), −2 (0x8939e734), −3 (0x8939e738) and 20 (0x8939e73c); the Time law 0x8939c310's
    // −63 (0x8939c570), 60 (0x8939c574), −60 (0x8939c578), 20 (0x8939c57c), −3 (0x8939c580), −5 (0x8939c584), 3
    // (0x8939c588), 5 (0x8939c58c), 127 (0x8939c590), its 1200, 63 and 12000 the formant law's words; and the Formant
    // law 0x8939c410's low hold, −12000 (0x8939c594)
    float pcmDepthTop = 0, pcmVelocityScale = 0, pcmGainScale = 0;
    float depthKnee = 0, depthOffset = 0, depthPer = 0, depthKneeLow = 0, depthSlopeLow = 0, depthPerLow = 0;
    float depthScale = 0;
    float timeLeast = 0, timeKnee = 0, timeKneeLow = 0, timeScale = 0, timeFromLow = 0, timeLeastOut = 0;
    float timeFrom = 0, timeMostOut = 0, timeBeam = 0, pcmFormantLeast = 0;
};

[[nodiscard]] bool vc2LoadOscConvert(const char* path);
bool vc2OscConvertLoaded();
const Vc2OscConvertTables& vc2OscConvertTables();
// The level curve's word at an index −128…+255
inline uint16_t vc2OscLevelWord(const Vc2OscConvertTables& t, int index) { return t.level[(size_t)(index + 128)]; }
