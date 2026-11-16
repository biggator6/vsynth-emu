// HIRESO's tables from rom.4: see Vc2HiresoTables.h (v213).  Every address is rom.4's.
#include "Vc2HiresoTables.h"
#include "Vc2RomTables.h"
#include <utility>

namespace {
constexpr Vc2Span kCentres { 0x8994069c, Vc2HiresoTables::kBands };   // [FW] .data
constexpr Vc2Span kAnalysisRate { 0x895613f4, Vc2HiresoTables::kDecimatorWords };        // [FW]
constexpr Vc2Span kDecimator { 0x895616d8, Vc2HiresoTables::kDecimatorWords };           // [FW]
constexpr Vc2Span kInterpolator { 0x89561700, Vc2HiresoTables::kInterpolatorWords };     // [FW]
constexpr Vc2Span kResonators { 0x8993fe84, Vc2HiresoTables::kBands * Vc2HiresoTables::kResonatorWords };   // [FW]
constexpr Vc2Span kEnergyScales { 0x8993fe78, Vc2HiresoTables::kGroups };   // [FW] .data
constexpr Vc2Span kGateRates { 0x893edba4, 2 };                             // [FW] the analysis's pool: up, then down
constexpr uint32_t kWindows = 0x8956141c;                                   // [FW] 32 words
constexpr Vc2Span kAttack { 0x895628d0, Vc2HiresoTables::kCurve }, kRelease { 0x89562a64, Vc2HiresoTables::kCurve };  // [FW]
constexpr Vc2Span kHoldLaw { 0x893f1c20, 3 }, kHoldCurve { 0x893f1c30, 2 };   // [FW] slot 6's pool, its pointer between
constexpr Vc2Span kWeightA { 0x8994059c, Vc2HiresoTables::kBands }, kWeightB { 0x8994061c, Vc2HiresoTables::kBands };
constexpr Vc2Span kWeightScale { 0x893f0d30, 1 };                           // [FW] the constructor's pool
constexpr Vc2Span kCarrier { 0x89940278, Vc2HiresoTables::kCarrierWords };   // [FW] .data
constexpr uint32_t kTemplates = 0x8956172c, kPivotAt = 4;   // [FW] a record's pivot, its second float
constexpr uint32_t kSamples = 0x895628ac;                   // [FW] the synthesis's groups' samples a block, 3 words
constexpr Vc2Span kRateScales { 0x895628b8, Vc2HiresoTables::kGroups }, kGlide { 0x893f106c, 3 };   // [FW]
// [FW] slot 5's caps: bands from 20 on take the release curve at the index of their load's `mov #4·index, r0`, every
// 8 bytes from 0x893f145c; the bands before, at 0
constexpr uint32_t kCapLoads = 0x893f145c, kCapStep = 8;
constexpr int kCappedFrom = 20;
constexpr uint16_t kMovR0 = 0xe000, kMovMask = 0xff00;

// Each capped band's index into the release curve, from slot 5's own loads; false when one isn't such a load
bool capsOf(Sh4& image, int32_t* cap) {
    for (int b = kCappedFrom; b < Vc2HiresoTables::kBands; ++b) {
        const uint16_t load = image.rd16(kCapLoads + kCapStep * (uint32_t)(b - kCappedFrom));
        if ((load & kMovMask) != kMovR0) return false;
        cap[b] = (load & 0xff) / 4;
    }
    return true;
}
constexpr uint32_t kWarpPool = 0x893f18d4;                            // [FW] slot 4's pool, these words in this order
constexpr float Vc2HiresoTables::* kWarp[] = {
    &Vc2HiresoTables::pivotBase, &Vc2HiresoTables::pivotStep, &Vc2HiresoTables::highNeg, &Vc2HiresoTables::high,
    &Vc2HiresoTables::lowNeg, &Vc2HiresoTables::low, &Vc2HiresoTables::a0, &Vc2HiresoTables::a1,
    &Vc2HiresoTables::a2, &Vc2HiresoTables::a3, &Vc2HiresoTables::under, &Vc2HiresoTables::fade,
    &Vc2HiresoTables::over,
};

struct Read : Vc2HiresoTables {
    bool read(Sh4& image) {
        const std::pair<Vc2Span, float*> floats[] = {
            { kCentres, centre }, { kAnalysisRate, analysisRate }, { kDecimator, decimator },
            { kInterpolator, interpolator }, { kResonators, resonator }, { kEnergyScales, energyScale },
            { kGateRates, gateRate }, { kAttack, attack }, { kRelease, release }, { kHoldLaw, &hold[kHoldScale] },
            { kHoldCurve, &hold[kHoldStep] }, { kWeightA, weightA }, { kWeightB, weightB },
            { kWeightScale, &weightScale }, { kCarrier, carrier }, { kRateScales, rateScale }, { kGlide, glide },
        };
        for (const auto& [span, out] : floats) vc2ReadFloats(image, span, out);
        resonatorAt = kResonators.at;
        carrierAt = kCarrier.at;
        templatesAt = kTemplates;
        for (int r = 0; r < kRecords; ++r) {
            const uint32_t record = kTemplates + kRecordBytes * (uint32_t)r;
            templatePivot[r] = std::bit_cast<float>(image.rd32(record + kPivotAt));
            vc2ReadFloats(image, { record + kRecordGains, kBands }, templateGain[r]);
        }
        for (int g = 0; g < kGroups; ++g) samples[g] = (int32_t)image.rd32(kSamples + 4 * (uint32_t)g);
        if (!capsOf(image, releaseCap)) return false;
        for (int b = 0; b < kBands; ++b) window[b] = (int32_t)image.rd32(kWindows + 4 * (uint32_t)b);
        uint32_t at = kWarpPool;
        for (float Vc2HiresoTables::* word : kWarp) {
            this->*word = std::bit_cast<float>(image.rd32(at));
            at += 4;
        }
        return true;
    }
};

constinit Vc2RomTables<Read> g_tables;
} // namespace

bool vc2LoadHireso(const char* path) { return g_tables.load(path); }
bool vc2HiresoLoaded() { return g_tables.loaded(); }
const Vc2HiresoTables& vc2HiresoTables() { return g_tables.t; }
