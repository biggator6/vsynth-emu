#pragma once
// The VC-2's unvoiced detector (v208), both vocoder engines' input stage: rom.4's 0x893ed040 on each 32-sample block of
// the mic, its part of the reset 0x893ecf60, Unvoice Detect from the settings (0x893ed030) and its two readings, the
// largest peak of the last eight blocks (0x893ed3d0) and whether the block is voiced (0x893ed3e0).  Bit for bit:
// xt_vc2in against rom.4 in Sh4Interp, every block's state word for word.  HARDWARE_FIRMWARE.md, "The VC-2's unvoiced
// detector"; research/VC2_VOCAL_DESIGNER.md, "The input stage".  Audio thread.
#include <array>
#include <cstdint>

// rom.4's two coefficient tables (0x895613a0, 10 floats; 0x895613c8, 5), read once by loadVc2 at the start
// (XtSynthRom.cpp), never after; a detector before them filters with zeros
[[nodiscard]] bool vc2LoadUnvoiced(const char* path);
bool vc2UnvoicedLoaded();

// The detector's state as rom.4 lays it out from 0x899d9978, word for word, the output stage's words among it (v209's)
struct Vc2UnvoicedWords {
    static constexpr int kBlock = 32, kPeaks = 8, kCounts = 4;
    int32_t detect;                      // 9978: Unvoice Detect + 24, from the settings
    int32_t spread;                      // 997c: Unvoice Spread (the output stage's)
    float soundLevel;                    // 9980: Vocoder Sound Level (the output stage's)
    float threshold;                     // 9984: the vote's, 29 once a block is unvoiced
    float bq1[2], bq2[2];                // 9988…9994: the highpass's two biquads' states
    float hysteresis;                    // 9998: the Schmitt trigger's ± a block
    float schmitt, schmittLast;          // 999c, 99a0: where the trigger stands
    float peaks[kPeaks];                 // 99a4…99c0: this block's peak, then the seven before
    float counts[kCounts];               // 99c4…99d0: this block's crossings (scaled), then three before
    float total;                         // 99d4: the vote's sum
    float outputWord99d8;                // 99d8: the output stage's
    float smooth[kBlock];                // 99dc…9a58: the decision, a sample at a time
    float max;                           // 9a5c: the largest peak of the last eight blocks
    float outputWords9a60[2];            // 9a60, 9a64: the output stage's
    float decaying, level;               // 9a68, 9a6c: a peak falling 1% a block, a level by attack and release
    int32_t outputIndex;                 // 9a70: the output stage's ring index
    int32_t ringIndex;                   // 9a74: where the next flag goes
    float bq3[2];                        // 9a78, 9a7c: the third biquad's
};
static_assert(sizeof(Vc2UnvoicedWords) == 0x108, "rom.4's layout, 0x899d9978…0x899d9a80");

class Vc2Unvoiced {
public:
    static constexpr int kBlock = Vc2UnvoicedWords::kBlock, kRing = 20;

    void reset();                         // 0x893ecf60's words of this region (its rings and the output stage's buffers aside)
    // 0x893ed030: Unvoice Detect + 24, Unvoice Spread and Vocoder Sound Level, as it keeps them (the last two the output
    // stage's)
    void settings(int detectPlus24, int spread, float soundLevel);
    void block(const float* in);          // 0x893ed040: 32 samples of the mic, read and not written
    float peak() const { return s.max; }  // 0x893ed3d0
    bool voiced() const;                  // 0x893ed3e0: neither the flag 1 block back nor 5 back says unvoiced
    const float* smoothed() const { return s.smooth; }

    Vc2UnvoicedWords s {};
    std::array<int32_t, kRing> flags {};   // 0x8a0f87dc: a block's 0/1, 20 blocks

private:
    float runSamples(const float* in);    // the filters and the trigger; the block's peak
    float countScaled(float m);           // this block's count, by the level
    void vote();
};
