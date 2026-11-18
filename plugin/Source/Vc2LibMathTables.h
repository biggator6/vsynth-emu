#pragma once
// The constants of rom.4's single-precision library routines (Vc2LibMath.h), read from rom.4 once by loadVc2 at the
// start (Vc2RomTables), never after.  HARDWARE_FIRMWARE.md, "The VC-2's logf and powf".
#include <array>

struct Vc2LibMathTables {
    float notANumber = 0;            // 0x898c56e8: the answer to what has none
    float zero = 0, minusZero = 0;   // 0x898c56ec, 0x898c571c
    float infinity = 0;              // 0x898c56f0
    float minusInfinity = 0;         // 0x898c5720
    float one = 0, minusOne = 0;     // 0x898c56f4, 0x898c5724
    float half = 0;                  // 0x898c56f8: logf's exponent is taken from the mantissa's middle
    float ln2 = 0;                   // 0x898c5718
    float perLn2 = 0;                // 0x898c5698: 1/ln 2, exp's argument in octaves
    float root2 = 0, minusRoot2 = 0;   // 0x89509b78, 0x89509b74: logf's own pool
    std::array<float, 8> logSeries {};   // 0x898c56a4…: logf's odd series in z², highest power first
    std::array<float, 7> expOver {};     // 0x898c5660…: exp's numerator, highest power first
    std::array<float, 7> expUnder {};    // 0x898c567c…: its denominator
};

[[nodiscard]] bool vc2LoadLibMath(const char* path);
bool vc2LibMathLoaded();
const Vc2LibMathTables& vc2LibMathTables();
