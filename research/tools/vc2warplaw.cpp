// HIRESO's formant map, as read from slot 4: vc2warplaw.h.  Each step is one SH-4 instruction's rounding.
#include "vc2warplaw.h"

#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON

namespace vc2 {

namespace {

struct Slopes {
    float low, high;   // the two segments' slopes: from the low end to the pivot, from the pivot to the high end
};

// The input frequency the warp sends to centre cb: on the segment cb falls in, x with anchor + slope (x − anchor) = cb
float sourceOf(const WarpConstants& k, float pivotOut, Slopes s, float cb) {
    const bool below = pivotOut > cb;
    const float slope = below ? s.low : s.high;
    const float anchor = below ? k.low : k.high;
    const float scaled = slope * anchor;
    const float offset = anchor - scaled;
    return (cb - offset) / slope;
}

// a1 + (a2 + a3 x) x: the cubic short of its last step, which the blends take in another order
float cubicInner(const WarpConstants& k, float x) {
    return fmac(fmac(x, k.cubic[3], k.cubic[2]), x, k.cubic[1]);
}

} // namespace

WarpConstants warpConstants(Machine& m) {
    WarpConstants k;
    for (int b = 0; b < kWarpBands; ++b) k.centre[(size_t)b] = m.f32(0x8994069c + 4 * (uint32_t)b);
    k.pivotBase = m.f32(0x893f18d4);
    k.pivotStep = m.f32(0x893f18d8);
    k.highNeg = m.f32(0x893f18dc);
    k.high = m.f32(0x893f18e0);
    k.lowNeg = m.f32(0x893f18e4);
    k.low = m.f32(0x893f18e8);
    for (int i = 0; i < 4; ++i) k.cubic[(size_t)i] = m.f32(0x893f18ec + 4 * (uint32_t)i);
    k.under = m.f32(0x893f18fc);
    k.fade = m.f32(0x893f1900);
    k.over = m.f32(0x893f1904);
    return k;
}

std::array<float, kWarpBands> warpModel(const WarpConstants& k, int p, float ratio) {
    const RoundToZero rz;
    const float pivot = fmac((float)p, k.pivotStep, k.pivotBase);
    const float pivotOut = pivot * ratio;
    const Slopes s{(pivotOut + k.lowNeg) / (pivot + k.lowNeg), (pivotOut + k.highNeg) / (pivot + k.highNeg)};
    std::array<float, kWarpBands> map{};
    const float x0 = sourceOf(k, pivotOut, s, k.centre[0]);
    const float first = fmac(cubicInner(k, x0), x0, k.cubic[0]);
    const bool blendDown = ratio <= 1.0f && ratio > k.under;
    const bool blendUp = ratio > 1.0f && k.over > ratio;
    if (blendDown || blendUp) {
        const float w = blendDown ? 1.0f - ratio : ratio + -1.0f;
        map[0] = (first + -0.0f) * w / k.fade + 0.0f;
        float band = 1.0f;
        for (int b = 1; b < kWarpBands; ++b) {
            const float x = sourceOf(k, pivotOut, s, k.centre[(size_t)b]);
            const float idx = cubicInner(k, x) * x - band + k.cubic[0];
            map[(size_t)b] = idx * w / k.fade + band;
            band = band + 1.0f;
        }
    } else {
        map[0] = first;
        for (int b = 1; b < kWarpBands; ++b) {
            const float x = sourceOf(k, pivotOut, s, k.centre[(size_t)b]);
            map[(size_t)b] = fmac(cubicInner(k, x), x, k.cubic[0]);
        }
    }
    return map;
}

} // namespace vc2
