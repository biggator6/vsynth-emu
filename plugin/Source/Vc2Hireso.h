#pragma once
// The VC-2's HIRESO engine, the object (v216): rom.4's constructor 0x893f0a10, and its slots: 0 the reset (0x893f0b70),
// 1 the analysis (0x893f0c50, Vc2HiresoAnalysis), 4 the formant warp (0x893f15d0, vc2HiresoMapOf), 5 the settings
// (0x893f13e0: Env Attack and Release, the bands' caps, Hold Dump Time, Vocoder Depth, the Formant Template's records
// and pivots), 6 the Hold pedal (0x893f1960) and 7 its level (0x893f1a00).  The synthesis, slots 2 and 3, comes next;
// the words only it reads are kept as rom.4 lays them out.  Bit for bit: xt_vc2hireso against rom.4's object in
// Sh4Interp, its words after every slot.  HARDWARE_FIRMWARE.md, "HIRESO's object"; research/VC2_VOCAL_DESIGNER.md,
// "HIRESO".  Audio thread, but for the constructor.
#include "Vc2HiresoAnalysis.h"
#include "Vc2HiresoWarp.h"

// The object's words from +0x004 to its rate filters, as rom.4 lays them out (+0x000 is its vtable)
struct Vc2HiresoWords {
    static constexpr int kBands = 32, kGroups = 3, kBankWords = 14, kSideWords = 128;
    int32_t edge[kGroups + 1];            // +0x0004: the groups' first bands, then the end
    uint32_t carrierTable[kGroups];       // +0x0014: rom.4's address of each group's carrier coefficients, kept
    float synthesis[2049];                // +0x0020: the synthesis's, which nothing here reads or writes
    float bank[kBands][kBankWords];       // +0x2024: the carrier bank's, a band's 14 (the reset clears its first 7)
    float bandWords[3][kBands];           // +0x2724: three of the synthesis's a band, which the reset clears
    float weight[kBands];                 // +0x28a4: each band's (−1)^(b+1) · A_b · B_b · 50 / (A_31 · B_31)
    // +0x2924: the left side's map (slot 4's), three more of the synthesis's a band (the reset clears them), then the
    // right side's map, kSideWords on
    float formant[kSideWords + kBands];
};
static_assert(sizeof(Vc2HiresoWords) == 0x2ba0, "rom.4's layout, +0x0004…+0x2ba4");

// The hold's words and those after, as rom.4 lays them out from +0x2d58
struct Vc2HiresoHoldWords {
    int32_t controller;   // +0x2d58: the Hold pedal's last 0…127
    float level;          // +0x2d5c: slot 7's, 1 less the hold's share
    float held;           // +0x2d60: the hold's coefficient, the release curve's (0 frozen, 1 none)
    float synthesis[3];   // +0x2d64: the synthesis's (0, 1, 0 at a reset)
};

// The settings' words, as rom.4 lays them out from +0x2cb4 (after the rate filters) to the object's end
struct Vc2HiresoSettingWords {
    int32_t pivot[2];          // +0x2cb4: each side's pivot field (slot 4's), 12 from the constructor
    float attack;              // +0x2cbc: Env Attack's coefficient
    float release[32];         // +0x2cc0: each band's Env Release coefficient, no faster than its cap
    uint32_t unused;           // +0x2d40: nothing here reads or writes it
    float depth;               // +0x2d44: Vocoder Depth ÷ 127
    float synthesis;           // +0x2d48: the synthesis's (1 at a reset)
    uint32_t templateAt[2];    // +0x2d4c: each side's template gains: rom.4's address of its record's, or a RAM curve's
    float dump;                // +0x2d54: Hold Dump Time ÷ 127
    Vc2HiresoHoldWords hold;   // +0x2d58
};
static_assert(sizeof(Vc2HiresoSettingWords) == 0x2d70 - 0x2cb4, "rom.4's layout, +0x2cb4…+0x2d70");

// The settings slot 5 reads, in the vocoder's settings struct as rom.4 lays it out (0x893f06a0's; the words between
// are the vocoder's, which slot 5 doesn't read)
struct Vc2HiresoSettings {
    uint32_t vocoderLow[4];     // +0x00…+0x0c
    int32_t pivot[2];           // +0x10: each side's pivot, or below 0 for its template's (slot 5 writes it here)
    uint32_t vocoderHigh[4];    // +0x18…+0x24
    int32_t attack, release;    // +0x28: Env Attack, Env Release, 0…100 (101 on: left as they stand)
    int32_t formantTemplate;    // +0x30: 0…15 two records, 16 on the RAM curves, below 0 left as it stands
    float depth;                // +0x34: Vocoder Depth ÷ 127
    float dump;                 // +0x38: Hold Dump Time ÷ 127
};
static_assert(sizeof(Vc2HiresoSettings) == 0x3c, "rom.4's settings struct, +0x00…+0x3c");

class Vc2Hireso {
public:
    static constexpr int kRates = 4;

    Vc2Hireso();                                  // 0x893f0a10, on memory of zeros
    void reset();                                 // slot 0: the analysis's clear, the synthesis's states, the hold's
    void analyze(const float* in, bool voiced) { analysis.block(in, voiced); }   // slot 1
    void formant(int side, float ratio);          // slot 4: the side's map from its pivot field and the ratio
    void settings(Vc2HiresoSettings& in);         // slot 5, which writes the pivots it takes into `in`
    void hold(int controller);                    // slot 6: the Hold pedal's 0…127
    float holdLevel() const { return t.hold.level; }   // slot 7

    Vc2HiresoAnalysis analysis;
    Vc2HiresoWords s {};
    // +0x2ba4: the rate filters, the first two on the object's decimators' table, the others on its interpolators'
    Vc2RateFilter rates[kRates] {};
    Vc2HiresoSettingWords t {};

private:
    void weights();                               // the constructor's band weights
    void formantTemplate(Vc2HiresoSettings& in);  // slot 5's Formant Template: the sides' gains and pivots
};
