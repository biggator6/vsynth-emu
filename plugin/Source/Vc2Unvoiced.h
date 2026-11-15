#pragma once
// The VC-2's unvoiced detector (v208) and its consonants (v209): both vocoder engines' input and output stages, around
// the engine on each 32-sample block of the mic.  The input stage, rom.4's 0x893ed040, with its part of the reset
// 0x893ecf60, Unvoice Detect from the settings (0x893ed030) and its two readings, the largest peak of the last eight
// blocks (0x893ed3d0) and whether the block is voiced (0x893ed3e0); the output stage, 0x893ed510 (mono) and 0x893ed690
// (stereo), the mic 448 samples late gated by the detector's flag and added to the engine's output, and the mic ring's
// write point (0x893ed410).  Bit for bit: xt_vc2in against rom.4 in Sh4Interp, every block's state word for word.
// HARDWARE_FIRMWARE.md, "The VC-2's unvoiced detector", "The VC-2's consonants"; research/VC2_VOCAL_DESIGNER.md, "The
// input stage", "The output stage".  Audio thread, but for the constructor.
#include <array>
#include <cstdint>
#include <vector>

// rom.4's two coefficient tables (0x895613a0, 10 floats; 0x895613c8, 5), read once by loadVc2 at the start
// (XtSynthRom.cpp), never after; a detector before them filters with zeros
[[nodiscard]] bool vc2LoadUnvoiced(const char* path);
bool vc2UnvoicedLoaded();

// The two stages' state as rom.4 lays it out from 0x899d9978, word for word
struct Vc2UnvoicedWords {
    static constexpr int kBlock = 32, kPeaks = 8, kCounts = 4;
    int32_t detect;                      // 9978: Unvoice Detect + 24, from the settings
    int32_t spread;                      // 997c: Unvoice Spread, the consonants' allpasses on or off
    float soundLevel;                    // 9980: Vocoder Sound Level, the engine's output's
    float threshold;                     // 9984: the vote's, 29 once a block is unvoiced
    float bq1[2], bq2[2];                // 9988…9994: the highpass's two biquads' states
    float hysteresis;                    // 9998: the Schmitt trigger's ± a block
    float schmitt, schmittLast;          // 999c, 99a0: where the trigger stands
    float peaks[kPeaks];                 // 99a4…99c0: this block's peak, then the seven before
    float counts[kCounts];               // 99c4…99d0: this block's crossings (scaled), then three before
    float total;                         // 99d4: the vote's sum
    float gate;                          // 99d8: the consonants' gate, gliding to the flag kLate back a sample at a time
    float smooth[kBlock];                // 99dc…9a58: the decision, a sample at a time
    float max;                           // 9a5c: the largest peak of the last eight blocks
    float highpass[2];                   // 9a60, 9a64: the consonants' filter's states
    float decaying, level;               // 9a68, 9a6c: a peak falling 1% a block, a level by attack and release
    int32_t micIndex;                    // 9a70: where the next block of the mic goes in the mic ring
    int32_t ringIndex;                   // 9a74: where the next flag goes
    float bq3[2];                        // 9a78, 9a7c: the third biquad's
};
static_assert(sizeof(Vc2UnvoicedWords) == 0x108, "rom.4's layout, 0x899d9978…0x899d9a80");

class Vc2Unvoiced {
public:
    static constexpr int kBlock = Vc2UnvoicedWords::kBlock, kRing = 20;
    static constexpr int kMicRing = 640, kAllpass = 50000;   // the mic ring's samples; an allpass's, each kept twice

    Vc2Unvoiced();                        // the buffers' memory: not on the audio thread
    void reset();                         // 0x893ecf60, but rom.4's two .data words below, which nothing resets
    // 0x893ed030: Unvoice Detect + 24, Unvoice Spread and Vocoder Sound Level, as it keeps them
    void settings(int detectPlus24, int spread, float soundLevel);
    float* micBlock() { return &mic[(size_t)s.micIndex]; }   // 0x893ed410: the part mixer writes the mic's block there
    void block(const float* in);          // 0x893ed040: 32 samples of the mic, read and not written
    float peak() const { return s.max; }  // 0x893ed3d0
    bool voiced() const;                  // 0x893ed3e0: neither the last block's flag nor the one four before it
    const float* smoothed() const { return s.smooth; }
    // 0x893ed510 (one channel) and 0x893ed690 (two): 32 frames of two words, the engine's output, each channel of a
    // frame × the gliding level plus the consonants × gain (the block driver's fr4: the engine's consonant gain × 2^31);
    // the gate's target is the flag kLate back × flagScale (fr5: the engine's slot 7); levelMod is 0x89997298's word
    void consonants(float* frames, int channels, float gain, float flagScale, float levelMod);

    Vc2UnvoicedWords s {};
    std::array<int32_t, kRing> flags {};   // 0x8a0f87dc: a block's 0/1, 20 blocks
    std::vector<float> mic;                // 0x8a0f73dc: the mic, 640 samples, read 448 late
    // 0x8a0f882c, 0x8a15a2ac: the two allpasses (delays 280 and 162), each sample written at its index and 50,000 on
    std::vector<float> allpassA, allpassB;
    float level = 0.0f;                    // 0x8993fe5c: Sound Level as it glides (rom.4's .data: 0 at the start)
    int32_t allpassAt = 0;                 // 0x8993fe58: the allpasses' index, falling a sample at a time (the same)

private:
    float runSamples(const float* in);    // the filters and the trigger; the block's peak
    float countScaled(float m);           // this block's count, by the level
    void vote();
    float lateFlag() const;               // the flag kLate back of the ring's index, as a float
    float filtered(float x, float target, float rate);   // the gate and the highpass on one sample of the late mic
};
