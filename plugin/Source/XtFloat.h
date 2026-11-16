#pragma once
// Single-precision arithmetic as the SH-4 runs it: FPSCR.RM = round to zero,
// or the P-code emulator's round to nearest with XT_ROUND=nearest (a harness's
// switch, XtSwitches.h; the plugin always rounds as the SH-4).  A
// product of two floats is exact in double, and a quotient is never within
// double rounding of a float boundary, so rounding the double once gives the
// SH-4's.  A sum is not always exact in double (operands more than about 29
// bits apart in exponent): it is rounded from the double sum and its exact
// error (TwoSum), so a tiny addend still moves a large value one unit toward
// zero, as the SH-4 does.  A square root (v215) is as a quotient: the double's
// root is never within double rounding of a float, so rounding it once gives
// the SH-4's FSQRT, which Sh4Interp takes under the host's round to zero.
#include "XtSwitches.h"
#include <cmath>
#include <cstdint>

namespace xtf {

// The host's denormals flushed, as the SH-4 does (FPSCR.DN = 1) and the plugin's audio thread does: a harness's first
// call, so its port and the interpreter both flush (v208; xt_tone, xt_analog and xt_fw still have the block inline)
inline void flushDenormals() {
#if defined(__aarch64__)
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));   // FPCR.FZ
#endif
}

inline bool nearest() { return xtSwitchNearest(); }
inline float rnd(double x) {
    float f = (float)x;
    if (!nearest() && std::fabs((double)f) > std::fabs(x)) f = std::nextafter(f, 0.0f);
    return f;
}
// The float of s + err, s the double sum and err its exact error.
inline float rndSum(double s, double err) {
    float f = (float)s;
    if (err == 0.0) return nearest() ? f : rnd(s);
    const double d = (double)f - s;                 // exact: f is within a unit of s
    if (nearest()) {
        // only an s exactly halfway between two floats can round the wrong
        // way: the error then picks the side
        if (d == 0.0) return f;
        const float g = std::nextafter(f, d > 0.0 ? -INFINITY : INFINITY);
        if (((double)f + (double)g) * 0.5 != s) return f;
        return (err > 0.0) == (f > g) ? f : g;
    }
    const bool pos = s > 0.0 || (s == 0.0 && err > 0.0);
    if (pos ? d > err : d < err) f = std::nextafter(f, 0.0f);   // f beyond the exact sum: one unit toward zero
    return f;
}
inline float sum2(float a, float b) {
    const double x = a, y = b, s = x + y, bb = s - x;
    return rndSum(s, (x - (s - bb)) + (y - bb));    // TwoSum
}
inline float mul(float a, float b) { return rnd((double)a * b); }
inline float div(float a, float b) { return rnd((double)a / b); }
inline float sub(float a, float b) { return sum2(a, -b); }
inline float add(float a, float b) { return sum2(a, b); }
inline float flt(int x) { return rnd((double)x); }   // FLOAT
inline float fsqrt(float x) { return rnd(std::sqrt((double)x)); }   // FSQRT
inline int ftrc(float x) {                            // FTRC (saturating)
    if (!(x == x)) return 0;
    if (x >= 2147483648.0f) return 0x7fffffff;
    if (x < -2147483648.0f) return (int)0x80000000;
    return (int)x;
}

} // namespace xtf
