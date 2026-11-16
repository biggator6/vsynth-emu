#pragma once
// The VC-2's HIRESO engine, the object (v216): rom.4's constructor 0x893f0a10, and its slots: 0 the reset (0x893f0b70),
// 1 the analysis (0x893f0c50, Vc2HiresoAnalysis), 2 the mono synthesis (0x893f0c60, v217), 3 the stereo (0x893f1240,
// v218), 4 the formant warp (0x893f15d0, vc2HiresoMapOf), 5 the settings (0x893f13e0: Env Attack and Release, the
// bands' caps, Hold Dump Time, Vocoder Depth, the Formant Template's records and pivots), 6 the Hold pedal (0x893f1960)
// and 7 its level (0x893f1a00).  Slots 2 and 3 are one routine here on one channel or two: rom.4's stereo is its mono
// on each channel, the samples and states interleaved.  Bit for bit: xt_vc2hireso against rom.4's object in Sh4Interp,
// its words after every slot.  HARDWARE_FIRMWARE.md, "HIRESO's object", "HIRESO's mono synthesis", "HIRESO's stereo
// synthesis"; research/VC2_VOCAL_DESIGNER.md, "HIRESO".  Audio thread, but for the constructor.
#include "Vc2Engine.h"
#include "Vc2HiresoAnalysis.h"
#include "Vc2HiresoWarp.h"

// A side's band words: each band's held envelope, its follower, and the target its ramp ends on
struct Vc2HiresoSideWords {
    float held[32], follower[32], target[32];
};

// The object's words from +0x004 to its rate filters, as rom.4 lays them out (+0x000 is its vtable)
struct Vc2HiresoWords {
    static constexpr int kBands = 32, kGroups = 3, kBankWords = 14, kRowWords = 64;
    int32_t edge[kGroups + 1];            // +0x0004: the groups' first bands, then the end
    uint32_t carrierTable[kGroups];       // +0x0014: rom.4's address of each group's carrier coefficients, which the
                                          // carrier bank reads through
    uint32_t unused;                      // +0x0020: nothing here reads or writes it
    // +0x0024: each band's carrier this block, its group's 8, 16 or 32 samples (stereo's pairs, left then right)
    float rows[kBands][kRowWords];
    // +0x2024: the carrier bank's: a group's from kBankWords × its first band, its last input (stereo's two), then each
    // resonator's two states (stereo's four, interleaved): a lower edge's, then each band's centre's and upper edge's;
    // the reset clears the first 7 of each 14
    float bank[kBands * kBankWords];
    Vc2HiresoSideWords left;              // +0x2724: mono's and stereo's left
    float weight[kBands];                 // +0x28a4: each band's (−1)^(b+1) · A_b · B_b · 50 / (A_31 · B_31)
    float mapLeft[kBands];                // +0x2924: the left side's map, slot 4's
    Vc2HiresoSideWords right;             // +0x29a4: stereo's right
    float mapRight[kBands];               // +0x2b24: the right side's map, 0x200 bytes on from the left's
};
static_assert(sizeof(Vc2HiresoWords) == 0x2ba0, "rom.4's layout, +0x0004…+0x2ba4");

// Each side's map and band words, by its number (0 the left, 1 the right): slot 4's side and a stereo channel
inline constexpr float (Vc2HiresoWords::*kVc2HiresoMaps[2])[Vc2HiresoWords::kBands] = {
    &Vc2HiresoWords::mapLeft, &Vc2HiresoWords::mapRight,
};
inline constexpr Vc2HiresoSideWords Vc2HiresoWords::*kVc2HiresoSides[2] = {
    &Vc2HiresoWords::left, &Vc2HiresoWords::right,
};

// The hold's words and those after, as rom.4 lays them out from +0x2d58
struct Vc2HiresoHoldWords {
    int32_t controller;   // +0x2d58: the Hold pedal's last 0…127
    float level;          // +0x2d5c: slot 7's, 1 less the hold's share
    float held;           // +0x2d60: the hold's coefficient, the release curve's (0 frozen, 1 none)
    float letGo;          // +0x2d64: the glide back after the hold, 1 while held, falling 0.01 a block to 0
    float take, keep;     // +0x2d68: this block's: a band's held envelope = envelope · take + held · keep
};

// The settings' words, as rom.4 lays them out from +0x2cb4 (after the rate filters) to the object's end
struct Vc2HiresoSettingWords {
    int32_t pivot[2];          // +0x2cb4: each side's pivot field (slot 4's), 12 from the constructor
    float attack;              // +0x2cbc: Env Attack's coefficient
    float release[32];         // +0x2cc0: each band's Env Release coefficient, no faster than its cap
    uint32_t unused;           // +0x2d40: nothing here reads or writes it
    float depth;               // +0x2d44: Vocoder Depth ÷ 127
    float synthesis;           // +0x2d48: 1 at a reset; neither synthesis reads it
    uint32_t templateAt[2];    // +0x2d4c: each side's template gains: rom.4's address of its record's, or a RAM curve's
    float dump;                // +0x2d54: Hold Dump Time ÷ 127
    Vc2HiresoHoldWords hold;   // +0x2d58
};
static_assert(sizeof(Vc2HiresoSettingWords) == 0x2d70 - 0x2cb4, "rom.4's layout, +0x2cb4…+0x2d70");

class Vc2Hireso final : public Vc2Engine {
public:
    static constexpr int kRates = 4;
    // The rate filters by their use in slots 2 and 3: the carrier 32 → 16 and 16 → 8, the output 16 → 32 and 8 → 16
    static constexpr int kToHalf = 0, kToQuarter = 1, kFromHalf = 2, kFromQuarter = 3;

    Vc2Hireso();                                  // 0x893f0a10, on memory of zeros
    void reset() override;                        // slot 0: the analysis's clear, the synthesis's states, the hold's
    void analyze(const float* in, bool voiced) override { analysis.block(in, voiced); }   // slot 1
    // slot 2 (v217): 32 frames of two words, the carrier in each first, in place; the second words left as they are
    void mono(float* frames) override { synthesis(frames, 1); }
    // slot 3 (v218): 32 frames of two words, a carrier in each, in place, the left's on the left side's words and the
    // right's on the right's
    void stereo(float* frames) override { synthesis(frames, 2); }
    void formant(int side, float ratio) override; // slot 4: the side's map from its pivot field and the ratio
    void settings(Vc2VocoderSettings& in) override;   // slot 5, which writes the pivots it takes into `in`
    void hold(int controller) override;           // slot 6: the Hold pedal's 0…127
    float holdLevel() const override { return t.hold.level; }   // slot 7

    Vc2HiresoAnalysis analysis;
    Vc2HiresoWords s {};
    // +0x2ba4: the rate filters, the first two on the object's decimators' table, the others on its interpolators'
    Vc2RateFilter rates[kRates] {};
    Vc2HiresoSettingWords t {};
    // Each side's template gains, where its word (t.templateAt) points: a record's in the tables or a RAM curve below
    const float* gains[2] {};
    // The RAM curves templates 16 on read (rom.4's 0x899d9d98, 0x899d9e18), which an edit page writes and nothing here
    float curves[2][Vc2HiresoWords::kBands] {};

private:
    void weights();                               // the constructor's band weights
    void formantTemplate(Vc2VocoderSettings& in); // slot 5's Formant Template: the sides' gains and pivots
    void synthesis(float* frames, int channels);  // slots 2 and 3, on 1 or 2 channels
    // 0x893f1a10 (stereo 0x893f1d40): a group's carrier bands, into their rows
    void carrierBank(const float* in, int group, int channels);
    void holdGlide();                             // slots 2's and 3's: this block's take and keep
    // 0x893f1b10 (slot 2's own for groups 1 and 0; stereo 0x893f1ec0): a group's bands on each channel's side's map,
    // band words and gains, into `out` (its samples a block, stereo's pairs)
    void synthesize(int group, float* out, int channels);
    float envelopeAt(float band) const;           // the analysis's envelope at a fractional band of a map
};
