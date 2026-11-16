// rom.4's shared math routines: see Vc2Math.h (v225).  Every address is rom.4's.
#include "Vc2Math.h"
#include "Vc2RomTables.h"
#include "XtFloat.h"

using xtf::mul;
using xtf::sub;

void vc2ReadMathPool(Sh4& image, Vc2MathPool& k) {
    const Vc2PoolWord words[] = {   // [FW] the routines' pool
        { 0x893f32ec, &k.foldAt }, { 0x893f3318, &k.foldNeg }, { 0x893f3310, &k.three }, { 0x893f3314, &k.four },
    };
    vc2ReadPool(image, words);
}

float vc2Cubic(const Vc2MathPool& k, float x) { return mul(sub(k.three, mul(mul(x, x), k.four)), x); }

float vc2Sine(const Vc2MathPool& k, float x) {
    if (x > k.foldAt) {
        x = sub(1.0f, x);
    } else if (k.foldNeg > x) {
        x = sub(-1.0f, x);
    }
    return vc2Cubic(k, x);
}

float vc2Cosine(const Vc2MathPool& k, float x) { return vc2Cubic(k, sub(k.foldAt, x < 0.0f ? -x : x)); }

float vc2Window(const Vc2MathPool& k, float x) {
    float u = mul(x, 2.0f);
    if (u > 1.0f) u = sub(2.0f, u);
    return mul(sub(1.0f, vc2Cubic(k, sub(k.foldAt, u))), k.foldAt);
}
