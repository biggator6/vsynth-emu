// The TVA's constants from rom.4: see Vc2TvaTables.h (v233).  Every address is rom.4's: a pool word its routine loads.
#include "Vc2TvaTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] the note-on's, the update's, the end tests' and the render's
auto tvaWords(Vc2TvaTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x893b2398, &t.onShortest }, { 0x893b239c, &t.onLongest }, { 0x893b23a0, &t.onPerMs },
        { 0x893b2628, &t.perSample }, { 0x893b262c, &t.upLongest }, { 0x893b2630, &t.upShortest },
        { 0x893b2634, &t.upPerMs }, { 0x893b2638, &t.upSamples }, { 0x893b263c, &t.floor },
        { 0x893b2840, &t.frames }, { 0x893b2844, &t.shortened }, { 0x893b2848, &t.renderFloor },
    });
}

using Tables = Vc2PoolTables<Vc2TvaTables, tvaWords>;
} // namespace

bool vc2LoadTva(const char* path) { return Tables::load(path); }
bool vc2TvaLoaded() { return Tables::loaded(); }
const Vc2TvaTables& vc2TvaTables() { return Tables::get(); }
