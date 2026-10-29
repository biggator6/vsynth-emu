#pragma once
// Single-precision arithmetic as the SH-4 runs it: FPSCR.RM = round to zero,
// or the P-code emulator's round to nearest with XT_ROUND=nearest.  One
// operation on two floats is exact in double (a quotient is never within
// double rounding of a float boundary), so rounding that result once gives
// the SH-4's.
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace xtf {

inline bool nearest() {
    static const bool n = [] { const char* r = std::getenv("XT_ROUND"); return r && std::strcmp(r, "nearest") == 0; }();
    return n;
}
inline float rnd(double x) {
    float f = (float)x;
    if (!nearest() && std::fabs((double)f) > std::fabs(x)) f = std::nextafter(f, 0.0f);
    return f;
}
inline float mul(float a, float b) { return rnd((double)a * b); }
inline float div(float a, float b) { return rnd((double)a / b); }
inline float sub(float a, float b) { return rnd((double)a - b); }
inline float add(float a, float b) { return rnd((double)a + b); }
inline float flt(int x) { return rnd((double)x); }   // FLOAT
inline int ftrc(float x) {                            // FTRC (saturating)
    if (!(x == x)) return 0;
    if (x >= 2147483648.0f) return 0x7fffffff;
    if (x < -2147483648.0f) return (int)0x80000000;
    return (int)x;
}

} // namespace xtf
