#pragma once
// The VC-2's HIRESO engine's analysis (v215): rom.4's 0x893ed930 on each 32-sample block of the mic, a function on .bss
// globals the engine's slot 1 (0x893f0c50) calls, with its init 0x893ed8f0 (the constructor's), slot 0's clear
// 0x893ede50, the envelopes' update 0x893edd10 (the synthesis's, slots 2 and 3, each block) and their getter
// 0x893eddf0.  The mic at three rates, through its two decimators (Vc2HiresoRate), each rate a group of bands: the
// group's first difference through each band's two resonators into the band's row; the voiced gate, gliding to the
// input stage's flag; each band's energy, the gated row's sum of squares by its group's scale, into a ring of 14
// frames; and each band's envelope, the root of its largest energy over its window.  Bit for bit: xt_vc2hireso against
// rom.4 in Sh4Interp, the state and the decimators word for word after every block, update and clear.
// HARDWARE_FIRMWARE.md, "HIRESO's analysis"; research/VC2_VOCAL_DESIGNER.md, "HIRESO".  Audio thread.
#include "Vc2HiresoRate.h"
#include "Vc2HiresoTables.h"
#include <cstdint>

// A two-pole resonator on the difference d, three coefficients c (a gain, then two feedbacks) and two states, st[0] and
// st[stride]: the analysis's bands' (v215) and the object's carrier bank's (v217; stereo's, its two channels' states
// interleaved, stride 2, v218), rom.4's arithmetic in each
float vc2HiresoResonate(const float* c, float* st, float d, int stride = 1);

// The analysis's state as rom.4 lays it out from 0x8a1bbd2c, word for word
struct Vc2HiresoAnalysisWords {
    static constexpr int kBands = 32, kBlock = 32, kFrames = 14, kGroups = 3, kBankStride = 7;
    int32_t edge[kGroups + 1];        // bd2c: each group's first band, then the end
    uint32_t table[kGroups];          // bd3c: rom.4's address of each group's coefficients, kept; the port reads them
                                      // from Vc2HiresoTables, where the three run on as these say
    uint32_t unused;                  // bd48: nothing here reads or writes it
    // bd4c: a group's from kBankStride × its first band: its last input, then each band's two resonators' two states
    float bank[kBankStride * kBands];
    float rows[kBands][kBlock];       // c0cc: each band's samples this block, at its group's rate
    float ring[kBands][kFrames];      // d0cc: each band's energy, by frame
    int32_t frame;                    // d7cc: this block's, 0…13
    float max[kBands];                // d7d0: each band's largest energy in its window
    float envelope[kBands];           // d850: its root
    int32_t age[kBands];              // d8d0: a countdown to the largest's leaving the window
    float gate[kBlock];               // d950: the voiced gate, a sample at a time
    float gateLevel;                  // d9d0: where it stands
};
static_assert(sizeof(Vc2HiresoAnalysisWords) == 0x1ca8, "rom.4's layout, 0x8a1bbd2c…0x8a1bd9d4");

class Vc2HiresoAnalysis {
public:
    using Words = Vc2HiresoAnalysisWords;
    static constexpr int kBands = Words::kBands, kBlock = Words::kBlock, kHalf = kBlock / 2, kQuarter = kBlock / 4;

    // 0x893ed8f0: all but the rows, the decimators' states and the mic at fs/2 and fs/4, which only the start
    // clears (0x893edf90, the file's static initializer, before any init)
    void reset();
    // 0x893ede50, slot 0's at a mode change: the bank, the ring and the envelopes' words, the frame and the gate kept
    void clear();
    // 0x893ed930: 32 samples of the mic, read and not written; `voiced` the input stage's 0x893ed3e0, which it asks
    void block(const float* in, bool voiced);
    void envelopes();                                             // 0x893edd10
    float envelope(int band) const { return s.envelope[band]; }   // 0x893eddf0

    Words s {};
    // As rom.4 lays them out from 0x899d9a88: the mic at fs/2 and fs/4, then the two decimators (each its table's
    // pointer, then its 16 states)
    float half[kHalf] {}, quarter[kQuarter] {};
    Vc2RateFilter toHalf { vc2HiresoTables().analysisRate, {} }, toQuarter { vc2HiresoTables().analysisRate, {} };

private:
    void bank(const float* in, int group);   // 0x893eda90: a group's bands, a sample at a time
    void gateTo(bool voiced);
    void energies(int group);                // 0x893edbd0, and the same for group 0 inline
    void rescan(int band);                   // the largest energy in the band's window again
};
