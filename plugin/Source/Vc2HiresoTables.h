#pragma once
// The VC-2's HIRESO engine's tables from rom.4 (v213; the engine's other parts add theirs as they come): the 32 bands'
// centres (0x8994069c, .data) and the formant warp's constants, slot 4's pool (0x893f18d4…0x893f1904); the rate
// filters' coefficients (v214); the analysis's (v215); the object's (v216).  Read once by loadVc2 at the start
// (Vc2RomTables); before them every map is 0 and the analysis gives 0.
#include <cstdint>

struct Vc2HiresoTables {
    static constexpr int kBands = 32, kDecimatorWords = 10, kInterpolatorWords = 11;
    static constexpr int kGroups = 3, kResonatorWords = 6;
    static constexpr int kCurve = 101, kRecords = 32, kCarrierWords = 3 * (2 * kBands + kGroups);
    static constexpr int kHoldScale = 0, kHoldFreeze = 1, kHoldBase = 2, kHoldStep = 3, kHoldGain = 4, kHoldWords = 5;
    // [FW] a Formant Template record: three floats (the pivot the second), then its 32 gains
    static constexpr uint32_t kRecordBytes = 140, kRecordGains = 12;
    // The object's (v216): Env Attack's and Env Release's curves (0x895628d0, 0x89562a64), 101 each; each band's index
    // into the release curve for its cap (0 for bands 0–19, then slot 5's own loads', 0x893f145c on); the hold's (slot
    // 6's pool from 0x893f1c20: 1/63, the freeze past 0.99, the curve's base 51 and step 50, its gain 32); the weights'
    // two tables (0x8994059c, 0x8994061c) and their scale (50, the constructor's pool); each Formant Template record's
    // pivot (from 0x8956172c, 140 bytes a record, the pivot its second float); the carrier's coefficients from
    // 0x89940278, three a resonator, a group's 2n + 1 in turn
    float attack[kCurve] {}, release[kCurve] {};
    int32_t releaseCap[kBands] {};
    float hold[kHoldWords] {};
    float weightA[kBands] {}, weightB[kBands] {}, weightScale = 0;
    float templatePivot[kRecords] {};
    uint32_t templatesAt = 0;   // where the records start in rom.4, which the object's words keep
    float carrier[kCarrierWords] {};
    uint32_t carrierAt = 0;     // the same for the carrier's coefficients
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

// The groups' first bands, then the end: fs/4, fs/2, fs ([FW] the immediates of the analysis's init 0x893edee0 and the
// object's constructor 0x893f0a10, both)
inline constexpr int32_t kVc2HiresoEdges[Vc2HiresoTables::kGroups + 1] = { 0, 22, 28, 32 };

[[nodiscard]] bool vc2LoadHireso(const char* path);
bool vc2HiresoLoaded();
const Vc2HiresoTables& vc2HiresoTables();
