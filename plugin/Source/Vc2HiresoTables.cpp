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
            { kGateRates, gateRate },
        };
        for (const auto& [span, out] : floats) vc2ReadFloats(image, span, out);
        resonatorAt = kResonators.at;
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
