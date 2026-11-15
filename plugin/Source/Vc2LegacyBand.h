#pragma once
// A LEGACY band's two resonators, as rom.4's analysis (0x893ef740) and its mono and stereo syntheses (0x893f25a0,
// 0x893f2980) run them on a band's five coefficients c and two of its states (v211, moved out of Vc2LegacyAnalysis.cpp
// as it stood; the states by reference from v212, as stereo keeps its channels' interleaved).  Each product is rounded
// before its sum (XtFloat).
#include "XtFloat.h"

// The first, in transposed form: y = c0·u + s0; s0 ← c1·y + s1; s1 ← c2·y
inline float vc2FirstResonator(const float* c, float& s0, float& s1, float u) {
    using namespace xtf;
    const float y = add(mul(c[0], u), s0);
    s0 = add(mul(c[1], y), s1);
    s1 = mul(c[2], y);
    return y;
}

// The second, its input taken whole: z = s2 + in; s2 ← c3·z + s3; s3 ← c4·z
inline float vc2SecondResonator(const float* c, float& s2, float& s3, float in) {
    using namespace xtf;
    const float z = add(s2, in);
    s2 = add(mul(c[3], z), s3);
    s3 = mul(c[4], z);
    return z;
}
