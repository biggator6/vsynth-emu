// The OSC voice's constants from rom.4: see Vc2OscTables.h (v230, v232).  Every address is rom.4's: a pool word its
// routine loads, or a float its install reads by a pool word's address.
#include "Vc2OscTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] the update's, the note-on's, the settings', the pitch getter's and the install's; then the targets'
auto oscWords(Vc2OscTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x893ae3d0, &t.waitLevel }, { 0x893ae3f4, &t.depthScale }, { 0x893ae48c, &t.plainHz },
        { 0x893ae628, &t.rateTop }, { 0x893ae630, &t.sampleHz }, { 0x893ae634, &t.cent },
        { 0x893ae638, &t.exponentBase }, { 0x893ae63c, &t.keyCents }, { 0x893ae640, &t.perOctave },
        { 0x893ae644, &t.tuneScale }, { 0x893ae87c, &t.settingsScale }, { 0x893aed7c, &t.getterCents.most },
        { 0x893aed80, &t.getterCents.least }, { 0x89542390, &t.installed4 }, { 0x895422f0, &t.installed5 },
        { 0x895422f4, &t.installed6 }, { 0x893afb38, &t.cents.most }, { 0x893afb3c, &t.cents.least },
        { 0x893afdc0, &t.ratio.perOctave }, { 0x893afdc8, &t.ratio.ratio.most }, { 0x893afdcc, &t.ratio.ratio.least },
        { 0x893afdd4, &t.ratio.cents.most }, { 0x893afdd8, &t.ratio.cents.least }, { 0x893af35c, &t.noteOn.cents.most },
        { 0x893af360, &t.noteOn.cents.least }, { 0x893af364, &t.noteOn.perOctave },
        { 0x893af36c, &t.noteOn.ratio.most }, { 0x893af370, &t.noteOn.ratio.least }, { 0x893af530, &t.kind6Least },
        { 0x893afddc, &t.walkTarget.most }, { 0x893afde0, &t.walkTarget.least }, { 0x893affd4, &t.walk.most },
        { 0x893affd8, &t.walk.least }, { 0x893afde4, &t.formantTargetLeast }, { 0x893affe0, &t.formantLeast },
        { 0x893af53c, &t.timesTenThousand }, { 0x893af7c8, &t.rateTenths },
    });
}
// [FW] the players' installs' tables (v238)
constexpr Vc2Span kLiteInstalled { 0x89542350, 6 }, kVariInstalled { 0x89542368, 10 };

struct Read : Vc2OscTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, oscWords(*this));
        vc2ReadFloats(image, kLiteInstalled, liteInstalled.data());
        vc2ReadFloats(image, kVariInstalled, variInstalled.data());
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadOsc(const char* path) { return tables.load(path); }
bool vc2OscLoaded() { return tables.loaded(); }
const Vc2OscTables& vc2OscTables() { return tables.t; }
