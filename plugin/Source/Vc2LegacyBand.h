#pragma once
// A LEGACY band's two resonators, as rom.4's analysis (0x893ef740) and synthesis (0x893f25a0) both run them on a band's
// five coefficients c and its state st (v211, moved out of Vc2LegacyAnalysis.cpp as it stood).  Each product is
// rounded before its sum (XtFloat).
#include "XtFloat.h"

// The first, in transposed form on st[0], st[1]: y = c0·u + s0; s0 ← c1·y + s1; s1 ← c2·y
inline float vc2FirstResonator(const float* c, float* st, float u) {
    using namespace xtf;
    const float y = add(mul(c[0], u), st[0]);
    st[0] = add(mul(c[1], y), st[1]);
    st[1] = mul(c[2], y);
    return y;
}

// The second, on st[2], st[3], its input taken whole: z = s2 + in; s2 ← c3·z + s3; s3 ← c4·z
inline float vc2SecondResonator(const float* c, float* st, float in) {
    using namespace xtf;
    const float z = add(st[2], in);
    st[2] = add(mul(c[3], z), st[3]);
    st[3] = mul(c[4], z);
    return z;
}
