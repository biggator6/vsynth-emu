// The part mixer's constants from rom.4: see Vc2PartMixerTables.h (v227).  Every address is rom.4's, each the pool word
// its routine loads.
#include "Vc2PartMixerTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] the mixer's, the level routine's and the targets' pools
auto mixerWords(Vc2PartMixerTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x893b1a88, &t.toFloat }, { 0x893b1aa8, &t.toRing }, { 0x893b1ab8, &t.ease[0] }, { 0x893b1d44, &t.ease[1] },
        { 0x893b15d8, &t.perLevel }, { 0x893b15f0, &t.pool[0].scale }, { 0x893b17d0, &t.pool[0].toBus },
        { 0x893b2068, &t.pool[1].scale }, { 0x893b2074, &t.pool[1].toBus }, { 0x893b17ec, &t.blockSeconds },
        { 0x893b17f4, &t.updateSeconds },
    });
}

using Tables = Vc2PoolTables<Vc2PartMixerTables, mixerWords>;
} // namespace

bool vc2LoadMixer(const char* path) { return Tables::load(path); }
bool vc2MixerLoaded() { return Tables::loaded(); }
const Vc2PartMixerTables& vc2MixerTables() { return Tables::get(); }
