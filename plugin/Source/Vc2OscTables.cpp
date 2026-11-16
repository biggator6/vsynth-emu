// The OSC voice's constants from rom.4: see Vc2OscTables.h (v230).  Every address is rom.4's: a pool word its routine
// loads, or a float its install reads by a pool word's address.
#include "Vc2OscTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] the update's, the note-on's, the settings', the pitch getter's and the install's
auto oscWords(Vc2OscTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x893ae3d0, &t.waitLevel }, { 0x893ae3f4, &t.depthScale }, { 0x893ae48c, &t.plainHz },
        { 0x893ae628, &t.rateTop }, { 0x893ae630, &t.sampleHz }, { 0x893ae634, &t.cent },
        { 0x893ae638, &t.exponentBase }, { 0x893ae63c, &t.keyCents }, { 0x893ae640, &t.perOctave },
        { 0x893ae644, &t.levelScale }, { 0x893ae87c, &t.settingsScale }, { 0x893aed7c, &t.most },
        { 0x893aed80, &t.least }, { 0x89542390, &t.installed4 }, { 0x895422f0, &t.installed5 },
        { 0x895422f4, &t.installed6 },
    });
}

using Tables = Vc2PoolTables<Vc2OscTables, oscWords>;
} // namespace

bool vc2LoadOsc(const char* path) { return Tables::load(path); }
bool vc2OscLoaded() { return Tables::loaded(); }
const Vc2OscTables& vc2OscTables() { return Tables::get(); }
