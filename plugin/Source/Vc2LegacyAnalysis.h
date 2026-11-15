#pragma once
// The VC-2's LEGACY engine's analysis (v210): rom.4's 0x893ef740 on each 32-sample block of the mic, a function on .bss
// globals the engine's slot 1 (0x893f2570) calls, with its init 0x893ef6a0 (the constructor's and slot 0's), its rows'
// reader 0x893efa00 (the synthesis's), and the hold: the controller 0x893efa70 (slot 6, CC 64), the dump time 0x893efaf0
// (slot 5) and the level 0x893efbe0 (slot 7).  Ten bands, each the mic's second difference through two resonators, a
// peak follower, the hold and a lowpass, into a row of 32 samples; the row after them is the unvoiced path, the mic's
// highs.  Bit for bit: xt_vc2legacy against rom.4 in Sh4Interp, the whole state word for word after every block and
// event.  HARDWARE_FIRMWARE.md, "LEGACY's analysis"; research/VC2_VOCAL_DESIGNER.md, "LEGACY: a 10-band channel
// vocoder".  Audio thread.
#include <cstdint>

// rom.4's tables: the bands' coefficients (five floats a band, at the .data pointer 0x8994024c), the band count
// (0x89940250) and the hold's coefficients (0x89561538), read once by loadVc2 at the start; before them no band runs
[[nodiscard]] bool vc2LoadLegacy(const char* path);
bool vc2LegacyLoaded();

// The analysis's state as rom.4 lays it out from 0x8a1bd9d4, word for word
struct Vc2LegacyAnalysisWords {
    static constexpr int kSlots = 32, kBandWords = 15, kBlock = 32, kPathWords = 15;
    // d9d4: a band's: 0, 1 the first resonator; 2, 3 the second; 8, 9 the lowpass; 11 the follower; 12 the held envelope
    float bands[kSlots][kBandWords];
    float last, beforeLast;            // e154, e158: the mic's last two samples
    float path[kPathWords];            // e15c: the unvoiced path's: 0 its lowpass, 1 its first difference, 2…7 its sections
    float rows[kSlots][kBlock];        // e198: each band's 32 samples, then the unvoiced path's
    float held;                        // f198: the hold's coefficient as the controller sets it (0 frozen, 1 none)
    float letGo;                       // f19c: the glide back after the hold, from 1 to 0 by 0.01 a block
    float take, keep;                  // f1a0, f1a4: this block's weights: held = envelope·take + held·keep
    int32_t controller;                // f1a8: the hold's last 0…127
};
static_assert(sizeof(Vc2LegacyAnalysisWords) == 0x17d8, "rom.4's layout, 0x8a1bd9d4…0x8a1bf1ac");

class Vc2LegacyAnalysis {
public:
    static constexpr int kBlock = Vc2LegacyAnalysisWords::kBlock;

    void reset();                          // 0x893ef6a0, which leaves the two .data words below
    void block(const float* in);           // 0x893ef740: 32 samples of the mic
    float row(int band, int n) const { return s.rows[band][n]; }   // 0x893efa00
    void hold(int controller);             // 0x893efa70 (slot 6): the Hold pedal's 0…127
    void dumpTime(float dump);             // 0x893efaf0 (slot 5): Hold Dump Time ÷ 127, on the controller as it stands
    float holdLevel() const { return level; }   // 0x893efbe0 (slot 7): 1 less the hold's share

    Vc2LegacyAnalysisWords s {};
    float dump = 1.0f;    // 0x89940254: rom.4's .data, 1 at the start [FW], set only by dumpTime
    float level = 1.0f;   // 0x89940258: the same, set by the hold

private:
    void holdFrom(int controller);         // the held coefficient and the level from the controller and dump time
    void weights();                        // this block's take and keep, from the controller and the glide back
    float band(int b, float u);            // one sample of a band, its row's sample
    float unvoiced(float x);               // one sample of the unvoiced path
};
