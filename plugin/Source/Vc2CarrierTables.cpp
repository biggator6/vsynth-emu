// The VOCODER OSC's constants from rom.4: see Vc2CarrierTables.h (v221, v222).  Every address is rom.4's, each the pool
// word its routine loads.
#include "Vc2CarrierTables.h"
#include "Vc2RomTables.h"

namespace {
// [FW] each routine's pool
auto carrierWords(Vc2CarrierTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x893ec6f8, &t.pitchFactor }, { 0x893ec228, &t.growlStart }, { 0x893ec22c, &t.noiseScale },
        { 0x893ec230, &t.plain.half }, { 0x893ec234, &t.plain.minusTwo }, { 0x893ec238, &t.plain.glide },
        { 0x893ec23c, &t.plain.toBus }, { 0x893ec4b8, &t.growl.half }, { 0x893ec4ac, &t.growl.minusTwo },
        { 0x893ec4bc, &t.growl.glide }, { 0x893ec4c0, &t.growl.toBus }, { 0x893ec4a0, &t.overPi },
        { 0x893ec4b0, &t.k440 }, { 0x893ec4b4, &t.k50 },
        { 0x893ec6c0, &t.lowBase }, { 0x893ec6c4, &t.lowSlope }, { 0x893ec6c8, &t.minPitch }, { 0x893ec6cc, &t.highA },
        { 0x893ec6d0, &t.highB }, { 0x893ec6d4, &t.highC }, { 0x893ec6d8, &t.floor }, { 0x893ec6e0, &t.dynHalf },
        { 0x893ec6e4, &t.dynLow }, { 0x893ec6e8, &t.dynSawBase }, { 0x893ec6ec, &t.dynSaw },
        { 0x893f3308, &t.clockScale }, { 0x893f330c, &t.logistic },
        { 0x8939e0a8, &t.perLevel }, { 0x8939ddd8, &t.velocityScale }, { 0x8939ddb4, &t.sumScale },
        { 0x8939e0b4, &t.growlScale }, { 0x8939e0b8, &t.keyfollowScale }, { 0x8939e0c0, &t.balanceScale },
        { 0x8939c2c0, &t.depthScale }, { 0x893b0254, &t.centsMax }, { 0x893b0258, &t.centsMin },
        { 0x893b025c, &t.perOctave }, { 0x893b0264, &t.ratioMax }, { 0x893b0268, &t.ratioMin },
    });
}

using Tables = Vc2PoolTables<Vc2CarrierTables, carrierWords>;
} // namespace

bool vc2LoadCarrier(const char* path) { return Tables::load(path); }
bool vc2CarrierLoaded() { return Tables::loaded(); }
const Vc2CarrierTables& vc2CarrierTables() { return Tables::get(); }
