// The TVA's words and block as the checks draw them: see Vc2TvaDraws.h (v233, shared from v317).  XT_VC2TVA_MAIN and
// XT_VC2TONE_MAIN only.
#if defined(XT_VC2TVA_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2TvaDraws.h"
#include "Vc2CheckRig.h"
#include "Vc2TvaTables.h"
#include <bit>
#include <cmath>

using std::bit_cast;

float tvaAboutUnit(std::mt19937& rng, float x) {
    const float picks[3] = { x, std::nextafter(x, INFINITY), std::nextafter(x, -INFINITY) };
    return picks[rng() % 3];
}

float tvaDrawFloat(std::mt19937& rng) {
    constexpr std::array<float, 6> kEdges { 0.0f, -0.0f, 1.0f, -1.0f, 3e38f, -3e38f };
    constexpr std::array<float, 4> kScales { 1.0f, 100.0f, 10000.0f, 0.001f };
    if (rng() % 4 == 0) return kEdges[rng() % kEdges.size()];
    return (2.0f * vc2Unit(rng) - 1.0f) * kScales[rng() % kScales.size()];
}

void tvaDrawBlockFloats(uint32_t* b, std::mt19937& rng) {
    // [FW] the words the TVA reads: the LFO's depth, the level, the pan LFO's depth, the word that puts the next two on
    // the gains when above 0 (about 0), and those two
    constexpr uint32_t kDepth = 0x08 / 4, kLevel = 0x0c / 4, kPanDepth = 0x1c / 4, kPanned = 0x40 / 4;
    constexpr uint32_t kLeft = 0x48 / 4, kRight = 0x4c / 4;
    for (uint32_t at : { kDepth, kPanDepth, kLeft, kRight }) b[at] = bit_cast<uint32_t>(tvaDrawFloat(rng));
    b[kLevel] = bit_cast<uint32_t>(rng() % 3 == 0 ? tvaDrawFloat(rng) : rng() % 2 ? 1.0f : vc2Unit(rng));
    const float panned[5] = { 0.0f, -0.0f, 1.0f, -1.0f, tvaAboutUnit(rng, 0.0f) };
    b[kPanned] = bit_cast<uint32_t>(panned[rng() % 5]);
}

std::array<uint32_t, kTvaBlockWords> tvaDrawBlock(std::mt19937& rng) {
    std::array<uint32_t, kTvaBlockWords> b {};
    for (uint32_t& word : b) word = (uint32_t)rng();
    tvaDrawBlockFloats(b.data(), rng);
    return b;
}

uint32_t tvaDrawSwitch(std::mt19937& rng) { return rng() % 4 != 0 ? 0 : rng() % 2 ? 1 : (uint32_t)rng(); }

float tvaDrawFade(std::mt19937& rng) {
    const float picks[4] = { tvaAboutUnit(rng, vc2TvaTables().floor), 0.0f, 1.0f, vc2Unit(rng) };
    return picks[rng() % 4];
}

float tvaDrawGain(std::mt19937& rng) {
    const float picks[5] = { tvaAboutUnit(rng, vc2TvaTables().floor), 1.0f, -1.0f, 2.0f * vc2Unit(rng) - 1.0f,
                             tvaDrawFloat(rng) };
    return picks[rng() % 5];
}

float tvaDrawTarget(std::mt19937& rng, float gain) {
    return rng() % 3 == 0 ? tvaAboutUnit(rng, gain) : tvaDrawGain(rng);
}

float tvaDrawStep(std::mt19937& rng, float target, float gain) {
    const float rest = xtf::sub(target, gain), exact = xtf::div(rest, vc2TvaTables().frames);
    const float picks[4] = { tvaAboutUnit(rng, exact), xtf::mul(exact, 0.5f),
                             xtf::mul(rest, vc2TvaTables().shortened), tvaDrawFloat(rng) };
    return picks[rng() % 4];
}

float tvaDrawElapsed(std::mt19937& rng) { return rng() % 4 == 0 ? tvaDrawFloat(rng) : (float)(32 * (rng() % 64)); }

uint32_t tvaDrawFadeStep(std::mt19937& rng, const Vc2TvaWords& w) {
    const float picks[5] = { xtf::div(w.fade, (float)(1 + rng() % 40)), xtf::sub(w.fade, vc2TvaTables().renderFloor),
                             0.0f, -0.001f * vc2Unit(rng), tvaDrawFloat(rng) };
    return bit_cast<uint32_t>(picks[rng() % 2 ? 0 : rng() % 5]);
}
#endif
