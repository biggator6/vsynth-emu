// The TB Filter's adapter (v255): its new block and note-on case over XtCosmTb on rom.4's object, see
// Vc2CosmAdapters.h.  Its start values are an rts (0x893b9130).  Every address is rom.4's, every offset the object's.
#include "Vc2CosmAdapters.h"
#include "XtCosm.h"
#include <algorithm>
#include <array>
#include <bit>
#include <utility>

namespace {
// [FW] the object as the TB Filter's routines name it: the block's +4…+0x43, the LPF's c0, c1, c2 and Q (D+0x1c), its
// states (D+0x2c), their targets ×0.01 (D+0x3c), the HPF's c0, c1 and the gain (D+0x5c), its states (D+0x68) and
// targets (D+0x78), the held levels (XtCosmTb's layout)
struct Vc2CosmTbWords {
    uint32_t head[5];                 // +0x000…+0x013: the set-up's words and the code
    std::array<uint32_t, 16> block;   // +0x014…+0x053
    uint32_t d054[8];
    float lpf[4];                     // +0x074
    float lpfState[4];                // +0x084
    float lpfTarget[4];               // +0x094
    uint32_t d0a4[4];
    float hpf[3];                     // +0x0b4
    float hpfState[4];                // +0x0c0
    float hpfTarget[3];               // +0x0d0
    uint32_t d0dc[10];
    float held[4];                    // +0x104: envelopes A and B, LFOs A and B
    uint32_t tail[20];                // +0x114…+0x163
};
static_assert(sizeof(Vc2CosmTbWords) == sizeof(Vc2CosmObjectWords), "rom.4's layout, the COSM object");

// [FW] the block's words XtTbBlock names, by word (Vc2CosmTbBlock): Gain +4, LPF Cutoff, Keyfollow, Q, Q Follow, HPF
// Cutoff and Keyfollow, the LPF's Env and LFO depths +0x20, +0x28, the HPF's +0x30, +0x38
constexpr std::array<std::pair<int, float XtTbBlock::*>, 11> kTbFloats { {
    { 0, &XtTbBlock::gain }, { 1, &XtTbBlock::lcut }, { 2, &XtTbBlock::lkf }, { 3, &XtTbBlock::q },
    { 4, &XtTbBlock::qf }, { 5, &XtTbBlock::hcut }, { 6, &XtTbBlock::hkf }, { 7, &XtTbBlock::lenv },
    { 9, &XtTbBlock::llfo }, { 11, &XtTbBlock::henv }, { 13, &XtTbBlock::hlfo } } };
constexpr int kTbKey = 15;   // [FW] the key, a signed byte at +0x40

XtTbBlock tbBlockOf(const std::array<uint32_t, 16>& words) {
    XtTbBlock x;
    for (const auto& [k, field] : kTbFloats) x.*field = std::bit_cast<float>(words[k]);
    x.key = static_cast<int8_t>(words[kTbKey]);
    return x;
}

// XtCosmTb's seven words (its coef() or target()) into the object's two places: the LPF's four, the HPF's three
void lpfAndHpf(float (&lpf)[4], float (&hpf)[3], const float* seven) {
    std::copy_n(seven, 4, lpf);
    std::copy_n(seven + 4, 3, hpf);
}
} // namespace

// The new block (rom.2's 0x894fc110): the block's +4…+0x43 into the object's, rom.4's own, then XtCosmTb's targets on
// the held levels (slot 1 the LPF's, slot 2 the HPF's) into theirs
void vc2CosmTbNewBlock(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    auto w = std::bit_cast<Vc2CosmTbWords>(o);
    std::copy_n(b.words, w.block.size(), w.block.begin());
    XtCosmTb tb;
    tb.setBlock(tbBlockOf(w.block), w.held[0], w.held[2], w.held[1], w.held[3]);
    lpfAndHpf(w.lpfTarget, w.hpfTarget, tb.target());
    o = std::bit_cast<Vc2CosmObjectWords>(w);
}

// The note-on case (the end of rom.2's 0x894fbd20): XtCosmTb's note-on on the object's block and held levels, its
// coefficients (the targets / 0.01) into the object's, and the states rom.4 clears (the frame clears the rest)
void vc2CosmTbNoteOnCase(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords&) {
    auto w = std::bit_cast<Vc2CosmTbWords>(o);
    XtCosmTb tb;
    tb.noteOn(tbBlockOf(w.block), w.held[0], w.held[2], w.held[1], w.held[3]);
    lpfAndHpf(w.lpf, w.hpf, tb.coef());
    std::ranges::fill(w.lpfState, 0.0f);
    std::ranges::fill(w.hpfState, 0.0f);
    o = std::bit_cast<Vc2CosmObjectWords>(w);
}
