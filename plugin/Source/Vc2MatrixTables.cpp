// rom.4's Matrix tables: see Vc2MatrixTables.h (v235, v245).  Every address is rom.4's.
#include "Vc2MatrixTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] the limits, u16 each by destination, and the sources' curve right after them (0x895401f6)
constexpr uint32_t kLimits = 0x89540158;

struct Read : Vc2MatrixTables {
    bool read(Sh4& image) {
        uint32_t at = kLimits;
        for (uint16_t& w : words) {
            w = static_cast<uint16_t>(image.rd16(at));
            at += sizeof w;
        }
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadMatrix(const char* path) { return tables.load(path); }
bool vc2MatrixLoaded() { return tables.loaded(); }
int32_t vc2MatrixLimit(int dest) { return tables.t.words[dest]; }
uint16_t vc2MatrixCurveWord(int i) { return tables.t.words[Vc2MatrixTables::kDestinations + i]; }
