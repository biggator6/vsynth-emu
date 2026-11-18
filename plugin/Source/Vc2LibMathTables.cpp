// The constants of rom.4's single-precision library routines: see Vc2LibMathTables.h.  Every address is rom.4's, the
// word its routine loads.
#include "Vc2LibMathTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr Vc2Span kLogSeries { 0x898c56a4, 8 }, kExpOver { 0x898c5660, 7 }, kExpUnder { 0x898c567c, 7 };   // [FW]

// [FW] each routine's words
auto libMathWords(Vc2LibMathTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x898c56e8, &t.notANumber }, { 0x898c56ec, &t.zero }, { 0x898c571c, &t.minusZero },
        { 0x898c56f0, &t.infinity }, { 0x898c5720, &t.minusInfinity }, { 0x898c56f4, &t.one },
        { 0x898c5724, &t.minusOne }, { 0x898c56f8, &t.half }, { 0x898c5718, &t.ln2 }, { 0x898c5698, &t.perLn2 },
        { 0x89509b78, &t.root2 }, { 0x89509b74, &t.minusRoot2 },
    });
}

struct Read : Vc2LibMathTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, libMathWords(*this));
        vc2ReadFloats(image, kLogSeries, logSeries.data());
        vc2ReadFloats(image, kExpOver, expOver.data());
        vc2ReadFloats(image, kExpUnder, expUnder.data());
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadLibMath(const char* path) { return tables.load(path); }
bool vc2LibMathLoaded() { return tables.loaded(); }
const Vc2LibMathTables& vc2LibMathTables() { return tables.t; }
