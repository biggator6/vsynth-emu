// LEGACY's tables from rom.4: see Vc2LegacyTables.h (v211).  Every address is rom.4's.
#include "Vc2LegacyTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] .data: the analysis's coefficients' pointer and its band count; the hold's coefficients; the synthesis's
constexpr uint32_t kCoefficientsAt = 0x8994024c, kCount = 0x89940250;
constexpr Vc2Span kHoldTable { 0x89561538, Vc2LegacyTables::kHoldEntries };
constexpr Vc2Span kCarrier { 0x89940744, Vc2LegacyTables::kCarrierBands * Vc2LegacyTables::kCoefficients };
constexpr Vc2Span kGains { 0x8994071c, Vc2LegacyTables::kCarrierBands };

struct Read : Vc2LegacyTables {
    bool read(Sh4& image) {
        count = (int)image.rd32(kCount);
        if (count < 0 || count >= kSlots) return false;
        vc2ReadFloats(image, { image.rd32(kCoefficientsAt), count * kCoefficients }, coefficients);
        vc2ReadFloats(image, kHoldTable, hold);
        vc2ReadFloats(image, kCarrier, carrier);
        vc2ReadFloats(image, kGains, gains);
        return true;
    }
};

constinit Vc2RomTables<Read> g_tables;
} // namespace

bool vc2LoadLegacy(const char* path) { return g_tables.load(path); }
bool vc2LegacyLoaded() { return g_tables.loaded(); }
const Vc2LegacyTables& vc2LegacyTables() { return g_tables.t; }
