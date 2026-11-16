// Voice kind 6's constants from rom.4: see Vc2Kind6Tables.h (v225).  Every address is rom.4's, each the pool word its
// routine loads.
#include "Vc2Kind6Tables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] each routine's pool
auto kind6Words(Vc2Kind6Tables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x893ec9d0, &t.gainScale }, { 0x893ec9d8, &t.glide }, { 0x893ec9fc, &t.perBlock },
        { 0x893eca00, &t.startDamping }, { 0x893eca04, &t.startWindow },
        { 0x893ecd24, &t.coreBlock }, { 0x893ecd30, &t.ringSpan }, { 0x893ecd34, &t.ringBack },
        { 0x893ecd3c, &t.blockAhead }, { 0x893ecd44, &t.liftGain }, { 0x893ecd48, &t.liftMost },
        { 0x893ecd4c, &t.liftRate },
        { 0x893ecf10, &t.span }, { 0x893ecf1c, &t.half }, { 0x893ecf20, &t.damping }, { 0x893ecf28, &t.fMost },
        { 0x893ecf2c, &t.filterGlide },
        { 0x893ecf40, &t.a440Period }, { 0x893ecf48, &t.unreadRate }, { 0x893ecf4c, &t.perA440 },
    });
}

using Tables = Vc2PoolTables<Vc2Kind6Tables, kind6Words>;
} // namespace

bool vc2LoadKind6(const char* path) { return Tables::load(path); }
bool vc2Kind6Loaded() { return Tables::loaded(); }
const Vc2Kind6Tables& vc2Kind6Tables() { return Tables::get(); }
