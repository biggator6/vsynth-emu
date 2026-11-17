// The VC-2's COSM constants from rom.4: see Vc2CosmTables.h (v234, v235).  Every address is rom.4's, each the pool word its
// routine loads.
#include "Vc2CosmTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] the Growl's DSP's pool and its update's; the TVF's converter's and the dispatcher's
auto cosmWords(Vc2CosmTables& t) {
    Vc2CosmGrowlPool& g = t.growl;
    return std::to_array<Vc2PoolWord>({
        { 0x893b9c80, &g.taps.three }, { 0x893b9c84, &g.fmScale }, { 0x893b9c88, &g.tiny },
        { 0x893b9c8c, &g.taps.foldAt }, { 0x893b9c90, &g.taps.four }, { 0x893b9c94, &g.line },
        { 0x893b9c98, &g.minusTwo }, { 0x893b9c9c, &g.rate }, { 0x893b9ca0, &g.taps.foldNeg },
        { 0x893b9ca4, &g.minusLine }, { 0x893b9e94, &g.speedFloor }, { 0x893b9e98, &g.k440 }, { 0x893b9e9c, &g.k50 },
        { 0x89398da0, &t.tvf.percent }, { 0x89398da4, &t.tvf.k127 }, { 0x89398db0, &t.tvf.k63 },
        { 0x89398db4, &t.tvf.qScale }, { 0x893999ac, &t.velocityScale },
    });
}

using Tables = Vc2PoolTables<Vc2CosmTables, cosmWords>;
} // namespace

bool vc2LoadCosm(const char* path) { return Tables::load(path); }
bool vc2CosmLoaded() { return Tables::loaded(); }
const Vc2CosmTables& vc2CosmTables() { return Tables::get(); }
