// xt_vc2cosm's second part's draws (v237, out of Vc2CosmFrameCheck.cpp, v235): a converter's input, its type mostly one
// the port has, each of that type's parameters in its range by its descriptor (rom.2's for the same type, the second
// bank: pnames.py), at its ends, 0, within, past them or any 16 bits; a parameter a converter reads the gain table at
// without a hold only within the table, as no stored setting is outside it (rom.4 reads whatever follows the table).
// XT_VC2COSM_MAIN only, and (v390) xt_vc2sets' COSM inputs' part's, XT_VC2SETS_MAIN.
#if defined(XT_VC2COSM_MAIN) || defined(XT_VC2SETS_MAIN)
#include "Vc2CosmCheck.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <span>

namespace {
// A parameter's range, and whether a converter reads the gain table at it as it stands
struct FrameRange {
    int lo, hi;
    bool index = false;
};
constexpr FrameRange kUnit { 0, 127 }, kDepth { -63, 63 }, kFollow { -20, 20 }, kIndex { 0, 127, true };

// Each ported type's parameters in its IDs' order
struct FrameTypeDraw {
    int type;
    std::span<const FrameRange> params;
};
constexpr std::array<FrameRange, 8> kAmp { { { 0, 2 }, kUnit, kUnit, kUnit, kIndex, kUnit, kDepth, kDepth } };
constexpr std::array<FrameRange, 2> kGrowl { { kUnit, kUnit } };
constexpr std::array<FrameRange, 11> kDual { { { 0, 2 }, kUnit, kFollow, kUnit, kDepth, kDepth, kUnit, kFollow,
                                               kUnit, kDepth, kDepth } };
constexpr std::array<FrameRange, 8> kTvf { { { 0, 11 }, kUnit, kDepth, kDepth, kUnit, kFollow, kDepth, kDepth } };
constexpr std::array<FrameRange, 10> kDtvf { { { 0, 11 }, kUnit, kDepth, kDepth, kDepth, kUnit, kFollow, kDepth,
                                               kDepth, kDepth } };
constexpr std::array<FrameRange, 3> kComp { { kUnit, kUnit, kUnit } };
constexpr std::array<FrameRange, 5> kLim { { kIndex, { 0, 3 }, kUnit, kUnit, kUnit } };
constexpr std::array<FrameRange, 11> kTb { { kUnit, kFollow, kDepth, kDepth, kUnit, kDepth, kUnit, kFollow, kDepth,
                                             kDepth, kIndex } };
// v239's: OD/Dist, the Wave Shaper (its Drive Env before its LFO), the Resonator, the Frequency Shifter, Lo-Fi
constexpr FrameRange kBalance { 0, 100 };
constexpr std::array<FrameRange, 9> kOd { { { 0, 1 }, kIndex, kUnit, kDepth, kDepth, kUnit, kFollow, kDepth, kDepth } };
constexpr std::array<FrameRange, 9> kWs { { { 0, 5 }, kIndex, kUnit, kDepth, kDepth, kUnit, kFollow, kDepth, kDepth } };
constexpr std::array<FrameRange, 8> kResonator { { { 0, 2 }, kUnit, kFollow, kDepth, kDepth, kBalance, kDepth,
                                                   kDepth } };
constexpr std::array<FrameRange, 7> kShifter { { kUnit, kFollow, kDepth, kDepth, kBalance, kDepth, kDepth } };
constexpr std::array<FrameRange, 9> kLofi { { { 0, 15 }, { 0, 5 }, kUnit, kFollow, kDepth, kDepth, kBalance, kDepth,
                                              kDepth } };
// v241's: the Side Band Filters (HPF or Q, LPF, its Keyfollow, Sub Detune, Octave; Detune and its depths, ±100; Width,
// its Keyfollow and depths)
constexpr FrameRange kDetune { -100, 100 };
constexpr std::array<FrameRange, 14> kSbf1 { { kUnit, kUnit, kFollow, kDetune, { 0, 2 }, kDetune, kDepth, kDepth,
                                               kDepth, kUnit, kFollow, kDepth, kDepth, kDepth } };
constexpr std::array<FrameRange, 13> kSbf2 { { kUnit, kUnit, kFollow, { 0, 2 }, kDetune, kDepth, kDepth, kDepth, kUnit,
                                               kFollow, kDepth, kDepth, kDepth } };
constexpr std::array<FrameTypeDraw, 15> kTypes { { { 1, kOd }, { 2, kWs }, { 3, kAmp }, { 5, kResonator },
                                                   { 6, kSbf1 }, { 7, kSbf2 }, { 8, kGrowl }, { 9, kDual },
                                                   { 10, kTvf }, { 11, kDtvf }, { 12, kComp }, { 13, kLim },
                                                   { 14, kShifter }, { 15, kLofi }, { 16, kTb } } };

// A setting in its range: its ends, 0, within, past them, or any 16 bits; a table's index its ends (the table's
// first and last words) or within
int16_t drawParam(std::mt19937& rng, const FrameRange& r) {
    const int span = r.hi - r.lo + 1;
    if (r.index) {
        const std::array<int, 3> picks { r.lo, r.hi, r.lo + (int)(rng() % (uint32_t)span) };
        return (int16_t)picks[rng() % picks.size()];
    }
    const std::array<int, 6> picks { r.lo, r.hi, 0, r.lo + (int)(rng() % (uint32_t)span),
                                     r.lo - 40 + (int)(rng() % (uint32_t)(span + 80)), (int16_t)rng() };
    return (int16_t)picks[rng() % picks.size()];
}

// A sensitivity: 0, the table's end and past it either way, within, or any 16 bits
int16_t drawSens(std::mt19937& rng) {
    const std::array<int, 8> picks { 0, 199, 200, -199, -200, 300, (int)(rng() % 399) - 199, (int16_t)rng() };
    return (int16_t)picks[rng() % picks.size()];
}
} // namespace

bool vc2CosmCheckPorted(int type) {
    return std::ranges::any_of(kTypes, [type](const FrameTypeDraw& t) { return t.type == type; });
}

void vc2DrawCosmInput(Vc2CosmInput& in, std::mt19937& rng, uint32_t matrixAt) {
    std::array<uint32_t, sizeof in / 4> noise {};
    for (uint32_t& w : noise) w = (uint32_t)rng();
    std::memcpy(&in, noise.data(), sizeof in);
    in.second = rng() % 2 ? 0 : (uint8_t)rng();
    in.on = rng() % 10 == 0 ? 0 : (uint8_t)(1 + rng() % 255);
    const FrameTypeDraw& ported = kTypes[rng() % kTypes.size()];
    const std::array<uint8_t, 4> types { (uint8_t)ported.type, (uint8_t)ported.type, (uint8_t)ported.type,
                                         (uint8_t)(rng() % 4 ? rng() % 18 : rng()) };
    in.type = types[rng() % types.size()];
    in.curveA = (uint8_t)(rng() % 10);
    in.curveB = (uint8_t)(rng() % 10);
    in.sensA = drawSens(rng);
    in.sensB = drawSens(rng);
    in.noteKey = (uint8_t)(rng() % 8 == 0 ? rng() : rng() % 128);
    in.noteVelocity = (uint8_t)(rng() % 8 == 0 ? rng() : rng() % 128);
    for (const FrameTypeDraw& t : kTypes)
        if (t.type == in.type)
            for (size_t k = 0; k < t.params.size(); ++k) in.params[k] = drawParam(rng, t.params[k]);
    in.matrix = matrixAt;
}
#endif
