// HIRESO's tables from rom.4: see Vc2HiresoTables.h (v213).  Every address is rom.4's.
#include "Vc2HiresoTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr Vc2Span kCentres { 0x8994069c, Vc2HiresoTables::kBands };   // [FW] .data
constexpr uint32_t kWarpPool = 0x893f18d4;                             // [FW] slot 4's pool, these words in this order
constexpr float Vc2HiresoTables::* kWarp[] = {
    &Vc2HiresoTables::pivotBase, &Vc2HiresoTables::pivotStep, &Vc2HiresoTables::highNeg, &Vc2HiresoTables::high,
    &Vc2HiresoTables::lowNeg, &Vc2HiresoTables::low, &Vc2HiresoTables::a0, &Vc2HiresoTables::a1,
    &Vc2HiresoTables::a2, &Vc2HiresoTables::a3, &Vc2HiresoTables::under, &Vc2HiresoTables::fade,
    &Vc2HiresoTables::over,
};

struct Read : Vc2HiresoTables {
    bool read(Sh4& image) {
        vc2ReadFloats(image, kCentres, centre);
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
