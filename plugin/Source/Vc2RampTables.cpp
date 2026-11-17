// The pitch ramp's constants from rom.4: see Vc2RampTables.h (v236).  Every address is rom.4's: a pool word its routine
// loads, or the tick's table.
#include "Vc2RampTables.h"
#include "Vc2RomTables.h"
#include <cstring>

namespace {
constexpr Vc2Span kRows { 0x89546044, 8 * 3 };   // [FW] the factor's curve, three floats a row
static_assert(sizeof(Vc2RampTables::rows) == 8 * 3 * sizeof(float), "the rows as rom.4 lays them out");

// [FW] the restart's (0x893b0b10), the legato update's (0x893b0e30), the constructor's (0x893b0dc0) and the tick's
// (0x893b0f30)
auto rampWords(Vc2RampTables& t) {
    Vc2RampTimePool& r = t.restart;
    Vc2RampTimePool& l = t.legato;
    return std::to_array<Vc2PoolWord>({
        { 0x893b0c30, &r.perOctave }, { 0x893b0c34, &r.slow }, { 0x893b0c38, &r.spanMost },
        { 0x893b0c3c, &r.spanLeast }, { 0x893b0c40, &r.fast }, { 0x893b0c44, &r.easeSlope },
        { 0x893b0d1c, &r.phaseRate }, { 0x893b0d20, &r.phaseEnd }, { 0x893b0d28, &t.restartPerCent },
        { 0x893b0f60, &t.factorLeast }, { 0x893b0f64, &t.factorMost },
        { 0x893b0f78, &l.perOctave }, { 0x893b0f7c, &l.slow }, { 0x893b0f80, &l.spanMost },
        { 0x893b0f84, &l.spanLeast }, { 0x893b0f88, &l.fast }, { 0x893b0f8c, &l.easeSlope },
        { 0x893b0f90, &l.phaseRate }, { 0x893b0f6c, &l.phaseEnd },
        { 0x893b0f6c, &t.builtPhase }, { 0x893b0f70, &t.builtRise }, { 0x893b0f74, &t.builtFall },
        { 0x893b1054, &t.factorEnd }, { 0x893b1058, &t.perPhase }, { 0x893b105c, &t.rowsPerX },
        { 0x893b1064, &t.factorFloor }, { 0x893b1068, &t.factorPerStep }, { 0x893b1210, &t.glideEnd },
        { 0x893b1218, &t.tickPerCent },
    });
}

struct Read : Vc2RampTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, rampWords(*this));
        std::array<float, 8 * 3> floats {};
        vc2ReadFloats(image, kRows, floats.data());
        std::memcpy(rows.data(), floats.data(), sizeof floats);
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadRamp(const char* path) { return tables.load(path); }
bool vc2RampLoaded() { return tables.loaded(); }
const Vc2RampTables& vc2RampTables() { return tables.t; }
