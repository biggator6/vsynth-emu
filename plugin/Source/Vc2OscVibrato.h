#pragma once
// The VC-2's OSC vibrato (v267): rom.4's own object on the OSC voice's first 0x54 bytes, word for word.  One of two
// waves of 513 floats read at a phase of 16 bits, with a delay, a rise and a hold; the build 0x893b29b0, the note-on
// 0x893b29e0, the settings 0x893b2b70 and the update 0x893b2c10, whose outputs are +0x2c (onto the pitch) and +0x30
// (onto the gains).  Bit for bit: xt_vc2osc's vibrato part against rom.4's routines in Sh4Interp.  HARDWARE_FIRMWARE.md,
// "The VC-2's OSC vibrato".  Audio thread.
#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr uint32_t kVc2VibratoWavesAt = 0x8993eb0c;   // [FW] the two waves, in .data
inline constexpr uint32_t kVc2VibratoWaveBytes = 0x804;      // [FW] 513 floats each
inline constexpr int kVc2VibratoWaves = 2, kVc2VibratoSamples = 513;

// The constants its routines read from rom.4 (Vc2OscTables.cpp)
struct Vc2OscVibratoPool {
    float stepPerRate = 0;   // 0x893b2d90: the static initializer's 0x8999f514, a rate into the phase's step
    float scaleDown = 0;     // 0x893b2d98: its 0x8999f518, the scale's slope below 100
    float scaleUp = 0;       // 0x893b2d9c: its 0x8999f51c, the scale's slope from 100
    float glide = 0;         // 0x893b2bf4: 0.07, the step's glide at a scale of 1
    float stepMost = 0;      // 0x893b2bf8: 65535
    float riseTen = 0;       // 0x893b2bfc: 10, over the rise
    float fallShort = 0;     // 0x893b2c00: 0.1, the fall's rate when its time is below 0
    float fallTen = 0;       // 0x893b2d80: 10, the fall's rate into the fade's
    float phaseTop = 0;      // [FW] 0x893b2d84: 65535, single precision as rom.4 keeps it
    float phaseBack = 0;     // [FW] 0x893b2d88: −65535, the same
    float perStep = 0;       // 0x893b2d8c: 1/512, between two samples of a wave
    std::array<float, (size_t)kVc2VibratoWaves * kVc2VibratoSamples> waves {};   // 0x8993eb0c
};

// What the note-on and the settings read: the OSC block's +0x80, as the converter 0x8939fbd0 writes it
struct Vc2VibratoBlock {
    int32_t wave;      // +0x00: which of the two waves
    float rate;        // +0x04: Vibrato Rate
    float pitchBias;   // +0x08: added to the wave for the pitch's output
    float levelBias;   // +0x0c: and for the level's
    float delay;       // +0x10
    float rise;        // +0x14
    float hold;        // +0x18
    float fall;        // +0x1c
    float stepRatio;   // +0x20: the step's target at the fall, as a ratio of its own
    int32_t scaleBy;   // +0x24: 100 is a scale of 1
};
static_assert(sizeof(Vc2VibratoBlock) == 0x28, "rom.4's layout, the block's +0x80…+0xa8");

// The object as rom.4 lays it out, word for word
struct Vc2VibratoWords {
    int32_t wave;          // +0x00
    float rate;            // +0x04
    float pitchBias;       // +0x08
    float levelBias;       // +0x0c
    float riseHold;        // +0x10: the rise and the hold, where the fall starts
    float time;            // +0x14: from below 0 through the delay, by 10 a count
    float fadeTarget;      // +0x18
    float fade;            // +0x1c
    float fadeRate;        // +0x20
    float fallRate;        // +0x24
    float stepRatio;       // +0x28
    float pitch;           // +0x2c: the output onto the pitch
    float level;           // +0x30: the output onto the gains
    int32_t active;        // +0x34
    int32_t falling;       // +0x38
    uint32_t waveAt;       // +0x3c: the wave's address in rom.4's memory
    float phase;           // +0x40: [FW] single precision as rom.4 keeps it; no note-on starts it, so it runs on
    float step;            // +0x44
    float stepTarget;      // +0x48
    float scale;           // +0x4c
    float glide;           // +0x50
};
static_assert(sizeof(Vc2VibratoWords) == 0x54, "rom.4's layout, the voice's +0x00…+0x54");

class Vc2OscVibrato {
public:
    void build();                              // 0x893b29b0, on the voice given
    void noteOn(const Vc2VibratoBlock& b);     // 0x893b29e0
    void settings(const Vc2VibratoBlock& s);   // 0x893b2b70: its rate and biases
    void update(int32_t n);                    // 0x893b2c10: n counts of 10 on the time

    Vc2VibratoWords w {};
};
