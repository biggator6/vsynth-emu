#pragma once
// C's envelopes', LFOs', mix's and TVA's constants from rom.4 (v282…v285): the note values' ticks, the sustain levels,
// the times by value, the LFOs' rates by value, the pool words of the envelope's block writer, release and key factor,
// of the LFO's depths, of the mix's gains and of the TVA's, read once by loadVc2 at the start (Vc2RomTables), never
// after.  HARDWARE_FIRMWARE.md, "The VC-2 part's sets' envelopes", "… LFOs", "… mix", "… TVA".
#include <array>
#include <cstdint>

struct Vc2SetEnvelopeTables {
    std::array<uint16_t, 128> notes {};    // 0x89542134: a note value's (128 on) ticks
    // 0x89541f34: a sustain level by its byte, then the mix's crossfade (0x89542034) and the note values' ticks, one
    // run, as a byte past 127 reads on (v284)
    std::array<uint16_t, 384> levels {};
    std::array<uint16_t, 128> times {};    // 0x8953fc90: a time by its value, 0…127
    float noteScale = 0;                   // 0x8939a66c: the block writer's note value's scale
    float releaseScale = 0;                // 0x8939a8f4: the release's
    float levelScale = 0;                  // 0x8939a67c: the sustain level's divisor
    float cents = 0;                       // 0x8939ab50: the key's factor's, an octave
    float unit = 0;                        // 0x8939ab58: its 16.16 one
    std::array<float, 128> rates {};       // 0x8953fdb0: an LFO's rate by its value, 0…127 (v283)
    float depthTop = 0;                    // 0x8939b624: an LFO depth's divisor
    float depthScale = 0;                  // 0x8939b628: its square's scale
    float mixOff = 0;                      // 0x8939bcf8: the mix's gains when it's off (v284)
    float mixOther = 0;                    // 0x8939bf08: when its type is none of the four
    float mixScale = 0;                    // 0x8939bd00: the level's and the crossfade's divisor
    float mixScaleTwo = 0;                 // 0x8939bf04: type 2's
    float tvaVelocity = 0;                 // 0x893a6938: the TVA's velocity curve's divisor (v285)
    float tvaScale = 0;                    // 0x893a64f0: its level run's divisor when on
    float tvaScaleOff = 0;                 // 0x893a6600: when off
    float tvaDepthTop = 0;                 // 0x893a64f4: its two depths' divisor
    float tvaSend = 0;                     // 0x893a6914: block[12]'s scale
    float tvaSixths = 0;                   // 0x893a6918: the gain's divisor, an octave
    float tvaSendTwo = 0;                  // 0x893a6920: block[14]'s scale
    float tvaRateScale = 0;                // 0x893a6928: the rate's divisor
    float tvaSampleRate = 0;               // 0x893a692c: its unit, kHz
    // The OSC blocks' (v391): the vibrato's rows from 0x8953ffd0, 80 bytes by Vibrato Type (its four, and the twelve
    // after them its four bits reach), the rate's scale (0x8939fd58, 6/127), the depth's (0x8939fd68, 300), and EXTIN's
    // start's tempo divisor (0x8939eb7c, 10; the analog's is Vc2OscConvertTables::perTenth)
    std::array<std::array<uint32_t, 20>, 16> vibrato {};
    float vibratoRate = 0, vibratoDepth = 0, extinTenth = 0;
};

// The level run's halfword at `at` over `scale`, as the SH-4 divides (the mix's and TVA's gains, v284, v285)
float vc2LevelRun(int32_t at, float scale);

[[nodiscard]] bool vc2LoadSetEnvelopes(const char* path);
bool vc2SetEnvelopesLoaded();
const Vc2SetEnvelopeTables& vc2SetEnvelopeTables();
