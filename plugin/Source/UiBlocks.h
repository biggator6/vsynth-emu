#pragma once
// D's blocks and pages as tables (research/UI_DESIGN.md, "The redesign, D"; UI step 1): the signal path's blocks in their
// order, which a click selects and the lower panel follows, and the page bar's pages.  An OSC and a COSM are instances,
// made from their counts and addressed by index; their parameters are the processor's (Params::oscId, Params::cosmId).
// A Block is a value (design/shape.json's "values"): a kind and an index, no container.
#include <array>

namespace Blocks {

enum class Kind { osc, mod, cosm, amp };
struct Block {
    Kind kind = Kind::osc;
    int index = 0;
};

inline constexpr int kOscs = 2, kCosms = 2;
inline constexpr int kCount = kOscs + 1 + kCosms + 1;

// The path in its order: the OSCs, MOD, the COSMs, AMP
constexpr std::array<Block, kCount> makePath() {
    std::array<Block, kCount> path {};
    int n = 0;
    for (int osc = 0; osc < kOscs; ++osc) path[(size_t)n++] = { Kind::osc, osc };
    path[(size_t)n++] = { Kind::mod, 0 };
    for (int cosm = 0; cosm < kCosms; ++cosm) path[(size_t)n++] = { Kind::cosm, cosm };
    path[(size_t)n] = { Kind::amp, 0 };
    return path;
}
inline constexpr std::array<Block, kCount> kPath = makePath();

// Where a block is on the path, -1 if it isn't
constexpr int indexOf(Kind kind, int index) {
    for (size_t i = 0; i < kPath.size(); ++i)
        if (kPath[i].kind == kind && kPath[i].index == index) return (int)i;
    return -1;
}

// The block a new editor shows selected: the first COSM, as the mock-up does
inline constexpr int kFirstSelected = indexOf(Kind::cosm, 0);

// The page bar's pages, in order; SYNTH is the path, the rack and the lower panel
inline constexpr std::array<const char*, 5> kPages = { "SYNTH", "MODULATION", "FX", "PLAY", "ZONES" };
inline constexpr int kSynthPage = 0, kFxPage = 2;

}
