// Auto Note's constants from rom.4: see Vc2AutoNoteTables.h (v226).  Every address is rom.4's, each the pool word its
// routine loads.
#include "Vc2AutoNoteTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] the bend's and the init's pools
auto autoNoteWords(Vc2AutoNoteTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x893ebd80, &t.smoothing }, { 0x893ebd84, &t.most }, { 0x893ebd88, &t.least },
        { 0x893ebe8c, &t.a440Period }, { 0x893ebe94, &t.unreadRate }, { 0x893ebe98, &t.c4Period },
    });
}

using Tables = Vc2PoolTables<Vc2AutoNoteTables, autoNoteWords>;
} // namespace

bool vc2LoadAutoNote(const char* path) { return Tables::load(path); }
bool vc2AutoNoteLoaded() { return Tables::loaded(); }
const Vc2AutoNoteTables& vc2AutoNoteTables() { return Tables::get(); }
