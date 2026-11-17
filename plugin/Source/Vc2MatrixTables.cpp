// rom.4's Matrix tables: see Vc2MatrixTables.h (v235).  Every address is rom.4's.
#include "Vc2MatrixTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr uint32_t kLimits = 0x89540158;   // [FW] the getter's, u16 each by destination

struct Read : Vc2MatrixTables {
    bool read(Sh4& image) {
        for (int d = 0; d < kDestinations; ++d) limits[d] = image.rd16(kLimits + 2 * (uint32_t)d);
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadMatrix(const char* path) { return tables.load(path); }
bool vc2MatrixLoaded() { return tables.loaded(); }
const Vc2MatrixTables& vc2MatrixTables() { return tables.t; }
