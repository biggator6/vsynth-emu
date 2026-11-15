// HIRESO's formant warp: see Vc2HiresoWarp.h (v213).  Each step is one SH-4 instruction's rounding (XtFloat), in the
// order slot 4 takes them; an FMAC is the product rounded, then the sum.  The check is xt_vc2hireso's
// (Vc2HiresoCheck.cpp).
#include "Vc2HiresoWarp.h"
#include "XtFloat.h"

namespace {
// The frequency the warp sends to centre cb, on the segment cb falls in: x with anchor + slope × (x − anchor) = cb
float sourceOf(const Vc2HiresoTables& t, float pivotOut, float lowSlope, float highSlope, float cb) {
    using namespace xtf;
    const bool below = pivotOut > cb;
    const float slope = below ? lowSlope : highSlope, anchor = below ? t.low : t.high;
    return div(sub(cb, sub(anchor, mul(slope, anchor))), slope);
}

// a1 + (a2 + a3 x) x: the cubic short of its last step, which the blend takes in another order
float cubicInner(const Vc2HiresoTables& t, float x) {
    using namespace xtf;
    return add(mul(add(mul(x, t.a3), t.a2), x), t.a1);
}
} // namespace

Vc2HiresoMap vc2HiresoMapOf(int pivotField, float ratio) {
    using namespace xtf;
    const Vc2HiresoTables& t = vc2HiresoTables();
    const float pivot = add(mul(flt(pivotField), t.pivotStep), t.pivotBase);
    const float pivotOut = mul(pivot, ratio);
    const float lowSlope = div(add(pivotOut, t.lowNeg), add(pivot, t.lowNeg));
    const float highSlope = div(add(pivotOut, t.highNeg), add(pivot, t.highNeg));
    const bool blendDown = ratio <= 1.0f && ratio > t.under, blendUp = ratio > 1.0f && t.over > ratio;
    const float weight = blendDown ? sub(1.0f, ratio) : add(ratio, -1.0f);
    Vc2HiresoMap map {};
    float band = 0.0f;   // the synthesis band, counted as the blend counts it: from 1 after band 0
    for (int b = 0; b < Vc2HiresoTables::kBands; ++b) {
        const float x = sourceOf(t, pivotOut, lowSlope, highSlope, t.centre[b]);
        const float inner = mul(cubicInner(t, x), x);
        if (!blendDown && !blendUp) map[(size_t)b] = add(inner, t.a0);
        else if (b == 0) map[0] = add(div(mul(add(add(inner, t.a0), -0.0f), weight), t.fade), 0.0f);
        else map[(size_t)b] = add(div(mul(add(sub(inner, band), t.a0), weight), t.fade), band);
        band = b == 0 ? 1.0f : add(band, 1.0f);
    }
    return map;
}
