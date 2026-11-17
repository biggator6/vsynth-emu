// xt_vc2cosm's fourth part's draws: see Vc2MatrixSetupDraws.h (v247).  Every address and ID is rom.4's.
#ifdef XT_VC2COSM_MAIN
#include "Vc2MatrixSetupDraws.h"

namespace {
// [FW] the parameters a record reads, by ID: Patch Category, the bend ranges below it, from 0x16a each Matrix Control's
// five (Source, Dest 1, Sens 1, Dest 2, Sens 2)
constexpr int16_t kCategoryId = 0x12c, kControlsId = 0x16a;
constexpr int kControlStep = 5;
enum class ParamKind { System, Category, Bend, Source, Dest, Sens };
constexpr std::array<ParamKind, kControlStep> kControlKinds { ParamKind::Source, ParamKind::Dest, ParamKind::Sens,
                                                              ParamKind::Dest, ParamKind::Sens };

ParamKind paramKindOf(int block, int16_t id) {
    if (block == kSetupSystem) return ParamKind::System;
    if (id >= kControlsId) return kControlKinds[(size_t)((id - kControlsId) % kControlStep)];
    return id == kCategoryId ? ParamKind::Category : ParamKind::Bend;
}

// Each kind's value: mostly what a store gives (its range less its centre), its edges, now and then past them.  The
// system's assignments 0…98 (0 and 32 the beams' none); Patch Category 0…11 (10 and 11 the categories that take
// velocity and keyfollow off Control 2); a bend range 0…48 (× 1000 past int16 from 33); a source 0…109 (32, 107, 108
// and the choice's cases); a destination 0…78; a sensitivity −63…63 (a 7-bit store's −64…63, and the tables' −128, 127)
using ParamDraw = int32_t (*)(std::mt19937&);
int32_t drawSystemParam(std::mt19937& rng) {
    const std::array<int32_t, 5> picks { 0, 32, (int32_t)(1 + rng() % 98), (int32_t)(rng() % 256), (int32_t)rng() };
    return picks[rng() % picks.size()];
}
int32_t drawCategory(std::mt19937& rng) {
    const std::array<int32_t, 4> picks { 10, 11, (int32_t)(rng() % 12), (int32_t)rng() };
    return picks[rng() % picks.size()];
}
int32_t drawBendRange(std::mt19937& rng) {
    const std::array<int32_t, 3> picks { (int32_t)(rng() % 49), (int32_t)(rng() % 49), (int32_t)rng() };
    return picks[rng() % picks.size()];
}
int32_t drawSourceValue(std::mt19937& rng) {
    const std::array<int32_t, 9> picks { 0, 32, (int32_t)(96 + rng() % 14), 107, 108, (int32_t)(rng() % 96),
                                         (int32_t)(110 + rng() % 18), -1 - (int32_t)(rng() % 4), (int32_t)rng() };
    return picks[rng() % picks.size()];
}
int32_t drawDestValue(std::mt19937& rng) {
    const auto within = (int32_t)(rng() % 79);
    const std::array<int32_t, 6> picks { within, within, 0, (int32_t)(79 + rng() % 49), -1 - (int32_t)(rng() % 4),
                                         (int32_t)rng() };
    return picks[rng() % picks.size()];
}
int32_t drawSensValue(std::mt19937& rng) {
    const auto within = (int32_t)(rng() % 128) - 64;
    const std::array<int32_t, 7> picks { 0, within, within, -64, 63, -128, (int32_t)(rng() % 256) - 128 };
    return picks[rng() % picks.size()];
}
constexpr std::array<ParamDraw, 6> kParamDraws { drawSystemParam, drawCategory, drawBendRange, drawSourceValue,
                                                 drawDestValue, drawSensValue };
} // namespace

int32_t SetupStore::value(int block, int16_t id) {
    const auto key = std::make_pair(block, (int)id);
    if (const auto at = values.find(key); at != values.end()) return at->second;
    const int32_t v = kParamDraws[(size_t)paramKindOf(block, id)](rng);
    values.emplace(key, v);
    return v;
}

SetupArgs vc2DrawSetupArgs(std::mt19937& rng) {
    const std::array<int32_t, 3> sources { 0, (int32_t)(1 + rng() % 109), (int32_t)rng() };
    const int32_t source = sources[rng() % sources.size()];
    const int32_t dest = drawDestValue(rng);
    return { source, dest, drawSensValue(rng) };
}

void vc2DrawSettings(Vc2MatrixSettings& g, std::mt19937& rng) {
    vc2CosmNoise(g, rng);
    for (Vc2MatrixFront& rec : g.parts)
        for (uint32_t& r : rec.routines) r = kVc2SourceRoutines[rng() % kVc2SourceRoutines.size()];
    g.channel = kSetupChannels[rng() % kSetupChannels.size()];
}
#endif
