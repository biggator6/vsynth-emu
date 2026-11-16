#pragma once
// The VC-2's HIRESO engine's tables from rom.4 (v213; the engine's other parts add theirs as they come): the 32 bands'
// centres (0x8994069c, .data) and the formant warp's constants, slot 4's pool (0x893f18d4…0x893f1904); the rate
// filters' coefficients (v214); the analysis's (v215).  Read once by loadVc2 at the start (Vc2RomTables); before them
// every map is 0 and the analysis gives 0.
#include <cstdint>

struct Vc2HiresoTables {
    static constexpr int kBands = 32, kDecimatorWords = 10, kInterpolatorWords = 11;
    static constexpr int kGroups = 3, kResonatorWords = 6;
    // The analysis's (v215): each band's two resonators, a gain and two feedbacks each, the three groups' bands in
    // order (0x8993fe84, .data); each group's energy scale (0x8993fe78, .data); the voiced gate's glide up and down
    // (0x893edba4, 0x893edba8, its pool); each band's envelope window in frames (0x8956141c)
    float resonator[kBands * kResonatorWords] {};
    uint32_t resonatorAt = 0;   // where they start in rom.4, which the analysis's words keep
    float energyScale[kGroups] {};
    float gateRate[2] {};   // up, then down
    int32_t window[kBands] {};
    // The rate filters' (v214): a decimator's ten (an input gain, then three biquads' three each) and an
    // interpolator's eleven (the same, then a one-pole's feedback); the analysis's decimators (0x895613f4), the
    // object's (0x895616d8), its interpolators (0x89561700)
    float analysisRate[kDecimatorWords] {}, decimator[kDecimatorWords] {}, interpolator[kInterpolatorWords] {};
    float centre[kBands] {};                    // the bands' centres, Hz: 100 to 11874
    float pivotBase = 0, pivotStep = 0;         // a side's pivot is base + step × its field: 500 + 172.27·p Hz
    float highNeg = 0, high = 0;                // the warp's fixed high end, 11874 Hz, and its negation
    float lowNeg = 0, low = 0;                  // its fixed low end, 100 Hz, and its negation
    float a0 = 0, a1 = 0, a2 = 0, a3 = 0;       // the cubic from Hz to a fractional band index
    float under = 0, fade = 0, over = 0;        // the blend's bounds and width: 0.95, 0.05, 1.05
};

[[nodiscard]] bool vc2LoadHireso(const char* path);
bool vc2HiresoLoaded();
const Vc2HiresoTables& vc2HiresoTables();
