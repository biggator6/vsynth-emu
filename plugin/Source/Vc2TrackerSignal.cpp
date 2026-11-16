// xt_vc2tracker's input: see Vc2TrackerSignal.h (v224).  XT_VC2TRACKER_MAIN only.
#ifdef XT_VC2TRACKER_MAIN
#include "Vc2TrackerSignal.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <numbers>

namespace {
constexpr double kFalloff = 0.6;     // each harmonic's amplitude, the last one's times this
constexpr double kHeadroom = 0.4;    // the partials' sum, at most 2.3, brought within full scale
constexpr float kLowestHz = 55.0f, kOctaves = 32.0f, kDecades = 4.0f;

// What a stretch mixes, and how many blocks it lasts: shortest, and up to spread more
struct StretchKind {
    float tone, noise;
    int shortest, spread;
};
constexpr StretchKind kKinds[] = {   // by Vc2TrackerStretch: tone, breathy, noise, silence, rough
    { 1.0f, 0.0f, 60, 940 }, { 1.0f, 0.3f, 60, 940 }, { 0.0f, 1.0f, 10, 200 }, { 0.0f, 0.0f, 10, 400 },
    { 0.0f, 0.0f, 20, 200 },
};
using enum Vc2TrackerStretch;
constexpr Vc2TrackerStretch kDraws[] = { tone, tone, tone, tone, breathy, noise, silence, rough };

const StretchKind& stretchOf(Vc2TrackerStretch s) { return kKinds[static_cast<int>(s)]; }
} // namespace

Vc2TrackerSignal::Vc2TrackerSignal(uint32_t seed, bool fades)
    : rng_(seed), fades_(fades), rough_ { std::mt19937(rng_()), false } {}

void Vc2TrackerSignal::draw() {
    const auto unit = [&] { return (float)rng_() / 4294967296.0f; };
    const bool fading = fades_ && drawn_ > 0;
    const bool step = !fades_ && stretchOf(kind_).tone > 0.0f && rng_() % 2 == 0;   // a new pitch, nothing else
    ++drawn_;
    if (step && rng_() % 2 == 0) {   // within half an octave
        hz_ = std::clamp(hz_ * std::exp2(unit() - 0.5f), kLowestHz, kLowestHz * kOctaves);
    } else {
        hz_ = kLowestHz * std::pow(kOctaves, unit());
    }
    if (step) {
        left_ = stretchOf(kind_).shortest + (int)(rng_() % (uint32_t)stretchOf(kind_).spread);
        return;
    }
    kind_ = fading ? silence : fades_ ? tone : kDraws[rng_() % std::size(kDraws)];
    const StretchKind& k = stretchOf(kind_);
    left_ = fading ? INT_MAX : k.shortest + (int)(rng_() % (uint32_t)k.spread);
    level_ = std::pow(10.0f, -kDecades * unit());
    partials_ = 1 + (int)(rng_() % kPartials);
}

void Vc2TrackerSignal::block(float* x) {
    if (--left_ <= 0) draw();
    if (kind_ == rough) {
        rough_.block(x);
        return;
    }
    const StretchKind& k = stretchOf(kind_);
    const double step = 2.0 * std::numbers::pi * hz_ / kVc2Rate;
    for (int i = 0; i < kVc2Block; ++i) {
        double sum = 0.0, amplitude = 1.0;
        for (int h = 0; h < partials_; ++h) {
            sum += amplitude * std::sin(phase_[h]);
            phase_[h] = std::fmod(phase_[h] + step * (h + 1), 2.0 * std::numbers::pi);
            amplitude *= kFalloff;
        }
        const double noise = 2.0 * rng_() / 4294967296.0 - 1.0;
        x[i] = (float)(level_ * (k.tone * kHeadroom * sum + k.noise * noise));
    }
}

float Vc2TrackerSignal::tonePeriod() const {
    return stretchOf(kind_).tone > 0.0f ? (float)kVc2Rate / hz_ : 0.0f;
}
#endif
