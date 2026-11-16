#pragma once
// rom.4's shared math routines (v225; the carrier's sine and cosine from v221): the cubic 0x893f31f0, x (3 − 4x²), a
// sine of πx on [−0.5, 0.5]; the sine 0x893f3210 and cosine 0x893f3280 around it; and the window 0x893f32a0, a
// raised cosine on [0, 1] made of it.  Each in rom.4's order, in XtFloat, on its pool's words, which each user's table
// reads with vc2ReadMathPool.  Bit for bit: xt_vc2carrier (the sine and cosine) and xt_vc2kind6 (the cubic and the
// window) against rom.4's in Sh4Interp.
class Sh4;

// The routines' pool, 0x893f32ec…0x893f331b
struct Vc2MathPool {
    float foldAt = 0;    // 0.5 (0x893f32ec): where the sine folds, and the cosine's and window's 0.5
    float foldNeg = 0;   // −0.5 (0x893f3318)
    float three = 0;     // 3 (0x893f3310)
    float four = 0;      // 4 (0x893f3314)
};

void vc2ReadMathPool(Sh4& image, Vc2MathPool& k);
float vc2Cubic(const Vc2MathPool& k, float x);    // 0x893f31f0: x (3 − 4x²)
float vc2Sine(const Vc2MathPool& k, float x);     // 0x893f3210: x in [−1, 1) folded into [−0.5, 0.5], then the cubic
float vc2Cosine(const Vc2MathPool& k, float x);   // 0x893f3280: the cubic on 0.5 − |x|
float vc2Window(const Vc2MathPool& k, float x);   // 0x893f32a0: ½ (1 − cubic(0.5 − u)), u = 2x folded about 1
