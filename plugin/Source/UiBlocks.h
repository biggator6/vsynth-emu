#pragma once
// D's blocks and pages as tables (research/UI_DESIGN.md, "The redesign, D"; UI step 1): the signal path's blocks in their
// order, which a click selects and the lower panel follows, and the page bar's pages.  An OSC and a COSM are instances,
// made from their counts and addressed by index; their parameters are the processor's (Params::oscId, Params::cosmId).
// A Block is a value (design/shape.json's "values"): a kind and an index, no container; and the mode's tables it is made
// from (v453: none, the V-Synth's; the VC-2's for a zone, UiVc2Kinds.h), which it points at and doesn't hold.
#include <array>

namespace Blocks {

struct Model;   // a mode's tables (UiBlockKinds.h)
enum class Kind { osc, mod, cosm, amp };
struct Block {
    Kind kind = Kind::osc;
    int index = 0;
    const Model* model = nullptr;
};

inline constexpr int kOscs = 2, kCosms = 2;
inline constexpr int kCount = kOscs + 1 + kCosms + 1;
using Path = std::array<Block, kCount>;

// The path in its order: the OSCs, MOD, the COSMs, AMP, each made from a mode's tables (none: the V-Synth's)
constexpr Path makePath(const Model* model = nullptr) {
    Path path {};
    int n = 0;
    for (int osc = 0; osc < kOscs; ++osc) path[(size_t)n++] = { Kind::osc, osc, model };
    path[(size_t)n++] = { Kind::mod, 0, model };
    for (int cosm = 0; cosm < kCosms; ++cosm) path[(size_t)n++] = { Kind::cosm, cosm, model };
    path[(size_t)n] = { Kind::amp, 0, model };
    return path;
}
inline constexpr Path kPath = makePath();

// Where a block is on the path, -1 if it isn't
constexpr int indexOf(Kind kind, int index) {
    for (size_t i = 0; i < kPath.size(); ++i)
        if (kPath[i].kind == kind && kPath[i].index == index) return (int)i;
    return -1;
}

// The block a new editor shows selected: the first COSM, as the mock-up does
inline constexpr int kFirstSelected = indexOf(Kind::cosm, 0);

// The page bar's pages, in order; SYNTH is the path, the rack and the lower panel
// (WAVE, v479: the XT's SAMPLE mode, an OSC's wave and its points; UiWavePage)
inline constexpr std::array<const char*, 6> kPages = { "SYNTH", "MODULATION", "FX", "PLAY", "ZONES", "WAVE" };
inline constexpr int kSynthPage = 0, kModulationPage = 1, kFxPage = 2, kPlayPage = 3, kZonesPage = 4, kWavePage = 5;
// The VC-2 mode's (v453; research/UI_DESIGN_VC2.md, d-1): its SYNTH is a zone's path, rack and lower panel
inline constexpr std::array<const char*, 6> kVc2Pages = { "SYNTH", "VOCODER", "MODULATION", "FX", "PLAY", "ZONES" };
inline constexpr int kVc2VocoderPage = 1, kVc2ModulationPage = 2, kVc2FxPage = 3, kVc2PlayPage = 4, kVc2ZonesPage = 5;

}
