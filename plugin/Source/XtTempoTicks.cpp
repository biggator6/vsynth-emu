// The tempo clock's ticks each ms: see XtTempoTicks.h.  Each line follows its routine's arithmetic, its widths and signs.
#include "XtTempoTicks.h"

namespace {
constexpr int32_t kUnitsAMs = 24;          // [FW 0x89471160] the speed × 24, in 16384ths
constexpr int kFracBits = 14;
constexpr int32_t kFracOne = 1 << kFracBits;
constexpr int32_t kRampAMs = 120 << 8;     // [FW 0x89471110] 120 ticks over six clocks' ms, in 256ths
constexpr int kRampBits = 8;
}

void XtTempoTicks::msInternal(int quarterMs) {
    const int32_t step = (int32_t)speed * kUnitsAMs;
    units += step >> kFracBits;
    const int32_t f = (int16_t)unitFrac + (step & (kFracOne - 1));
    unitFrac = (uint16_t)f;
    if (unitFrac >= kFracOne) {
        ++units;
        unitFrac = (uint16_t)((int16_t)unitFrac - kFracOne);
    }
    const int32_t d = units - unitsTaken;
    unitsTaken = units;
    const int32_t p = d * kPerClock;
    int32_t q = p / quarterMs;
    const int32_t r = (int16_t)remainder + p % quarterMs;
    remainder = (uint16_t)r;
    if ((int32_t)remainder >= quarterMs) {
        remainder = (uint16_t)(remainder - quarterMs);
        ++q;
    }
    ticks += (uint32_t)q;
}

void XtTempoTicks::msExternal(int six, int since) {
    if (since >= six) return;
    const int32_t r = (int16_t)ramp + kRampAMs / six;
    ramp = (uint16_t)r;
    const int32_t past = ramp >> kRampBits;
    if (past >= kPerClock) return;
    ticks = ticks - ticks % kPerClock + (uint32_t)past;
}

void XtTempoTicks::f8() {
    ticks = ticks - ticks % kPerClock + kPerClock;
    ramp = 0;
}

int32_t XtTempoTicks::take() {
    const int32_t d = (int32_t)(ticks - fed);
    fed = ticks;
    return d;
}
