#pragma once
// rom.4's single-precision library routines, bit for bit, on XtFloat: logf and powf, which the VC-2's converters and
// ramps call, and the two routines powf is made of.  Each is the routine's answer in fr0; the wrappers' errno
// (0x89add990: 1101 for a NaN answer, 1100 for an infinite one) follows from the answer and is left out.  The host's
// denormals are flushed, as the SH-4's are (xtf::flushDenormals; the audio thread's).  The tables are vc2LoadLibMath's.
// HARDWARE_FIRMWARE.md, "The VC-2's logf and powf".
#include <cstdint>

float vc2LibLog(float x);            // 0x895065f4, its core 0x89509a78 (rom.2's 0x89531b78)
float vc2LibPow(float x, float y);   // 0x89506e54, its core 0x89509b80
float vc2LibExp(float x);            // 0x89509928
float vc2LibLdexp(float x, int32_t n);   // 0x8950ae24: x × 2^n
