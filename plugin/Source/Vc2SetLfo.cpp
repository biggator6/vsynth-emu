// The VC-2 part's conversion sets' LFOs: see Vc2SetLfo.h (v283).  Each routine's reads, calls and writes in rom.4's
// order; the floats through XtFloat, as the SH-4 rounds.
#include "Vc2SetLfo.h"
#include "Vc2SetEnvelopeTables.h"
#include "XtFloat.h"
#include <algorithm>
#include <bit>

namespace {
constexpr int32_t kNoteFrom = 128, kTop = 127;    // [FW] a rate from 128 on is a note value; a sum's most
constexpr uint32_t kLastSummed = 4;               // [FW] the LFOs with a Matrix sum on their rate
// [FW] the waveform's code by its byte (0x8939b3b0): 1…7, else 1
constexpr std::array<uint32_t, 8> kWaves { 1, 0, 2, 3, 6, 4, 5, 7 };
constexpr uint32_t kFirstSwitchMost = 4, kFirstSwitchElse = 2;    // [FW] 0x8939b510: 0…4 as is, else 2
constexpr uint32_t kSecondSwitchMost = 3;                         // [FW] 1…3 as is, else 0

// [FW] each LFO's rate sum: the group's reader and the sum's offset
struct RateSum {
    uint32_t routine;
    uint32_t offset;
};
constexpr RateSum kRateSums[] = {
    { 0x893a26b0, 0 }, { 0x893a28b0, 0 }, { 0x893a2960, 0x10 }, { 0x893a2aa0, 0x10 }, { 0x893a2bf0, 6 },
};

const Vc2SetEnvelopeTables& lfoTables() { return vc2SetEnvelopeTables(); }

// 0x8939b450 below a note value (its only caller tests that first): the rate plus the LFO's Matrix sum, held to 0…127
uint32_t lfoRate(int16_t rate, uint32_t matrix, uint32_t index, Vc2SetReads& reads) {
    int32_t sum = 0;
    if (index <= kLastSummed) sum = reads.matrixSum(kRateSums[index].routine, matrix, kRateSums[index].offset);
    return static_cast<uint32_t>(std::clamp(static_cast<int32_t>(rate) + sum, 0, kTop));
}

// A depth's byte as the block takes it: (x / 127)² × 10000
float depthOf(uint8_t x) {
    const float q = xtf::div(xtf::flt(x), lfoTables().depthTop);
    return xtf::mul(xtf::mul(q, q), lfoTables().depthScale);
}

uint32_t firstSwitch(uint8_t b) { return b <= kFirstSwitchMost ? b : kFirstSwitchElse; }

uint32_t secondSwitch(uint8_t b) { return b >= 1 && b <= kSecondSwitchMost ? b : 0; }
} // namespace

void Vc2SetLfos::blockOf(const Vc2LfoWords& w, uint32_t* block, Vc2SetReads& reads) {
    const bool note = w.rate >= kNoteFrom;
    block[5] = note ? 1 : 0;
    block[1] = floatWord(note ? xtf::flt(lfoTables().notes[w.rate - kNoteFrom])
                              : lfoTables().rates[lfoRate(w.rate, w.matrix, w.index, reads)]);
    block[0] = w.wave < kWaves.size() ? kWaves[w.wave] : 1;
    block[6] = firstSwitch(w.byte0a);
    block[2] = floatWord(depthOf(w.depthA));
    block[7] = secondSwitch(w.byte0c);
    block[3] = floatWord(depthOf(w.depthB));
    block[4] = w.sync != 0;
}

void Vc2SetLfos::construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads) {
    Vc2LfoWords& w = at(c.object);
    vc2ChildMade(vc2ChildBytes(w), c, reads);
    w.matrix = c.matrix;
    vc2ChildPart(vc2ChildBytes(w), &Vc2SetReads::lfoOf, c.call, reads);
    blockOf(w, block, reads);
}
