// The VC-2's COSM constants from rom.4: see Vc2CosmTables.h (v234, v235, v237, v239).  Every address is rom.4's, each
// the pool word its routine loads.
#include "Vc2CosmTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr uint32_t kGainTable = 0x89541f34;   // [FW] the converters' gain table, u16 each
// [FW] the COSM object's install (v249): its DSP routines, mono and stereo, and its floats
constexpr uint32_t kMonoRoutines = 0x8993fb50, kStereoRoutines = 0x8993fba4, kObjectFactors = 0x895423b4;

// A table of routines read about its start, as an install reads it by a signed byte
void readRoutines(Sh4& image, uint32_t table, uint32_t* out) {
    for (int i = -Vc2CosmObjectPool::kCodeReach; i < Vc2CosmObjectPool::kCodeReach; ++i)
        out[i + Vc2CosmObjectPool::kCodeReach] = image.rd32(table + 4 * (uint32_t)i);
}

// [FW] the Growl's DSP's pool and its update's; the TVF's converter's and the dispatcher's; the batches of v237 and
// v239 (a helper's word read into each converter's pool that takes it)
auto cosmWords(Vc2CosmTables& t) {
    Vc2CosmGrowlPool& g = t.growl;
    return std::to_array<Vc2PoolWord>({
        { 0x893b9c80, &g.taps.three }, { 0x893b9c84, &g.fmScale }, { 0x893b9c88, &g.tiny },
        { 0x893b9c8c, &g.taps.foldAt }, { 0x893b9c90, &g.taps.four }, { 0x893b9c94, &g.line },
        { 0x893b9c98, &g.minusTwo }, { 0x893b9c9c, &g.rate }, { 0x893b9ca0, &g.taps.foldNeg },
        { 0x893b9ca4, &g.minusLine }, { 0x893b9e94, &g.speedFloor }, { 0x893b9e98, &g.k440 }, { 0x893b9e9c, &g.k50 },
        { 0x89398da0, &t.tvf.percent }, { 0x89398da4, &t.tvf.k127 }, { 0x89398db0, &t.tvf.k63 },
        { 0x89398db4, &t.tvf.qScale }, { 0x893999ac, &t.velocityScale },
        { 0x89399020, &t.dtvf.percent }, { 0x89399024, &t.dtvf.k127 }, { 0x89399030, &t.dtvf.k63 },
        { 0x89399034, &t.dtvf.qScale }, { 0x89398b4c, &t.dual.qScale }, { 0x89398b50, &t.dual.k63 },
        { 0x89397920, &t.dual.percent }, { 0x89397924, &t.dual.k127 }, { 0x893980b4, &t.amp.bass },
        { 0x893980c0, &t.amp.gainScale }, { 0x893976b0, &t.amp.k63 }, { 0x89397920, &t.levelGain.percent },
        { 0x89397bc0, &t.levelGain.over }, { 0x89397bc4, &t.levelGain.overGain }, { 0x89399270, &t.limiterScale },
        { 0x8939979c, &t.tb.lpfFollow }, { 0x893997a0, &t.tb.lpfFollowFrom }, { 0x893997a4, &t.tb.k63 },
        { 0x893999a0, &t.tb.hpfFollow }, { 0x893999a4, &t.tb.hpfFollowFrom }, { 0x893999ac, &t.tb.gainScale },
        { 0x89397bcc, &t.od.gainScale }, { 0x893976b0, &t.od.k63 }, { 0x893976b4, &t.od.key.scale },
        { 0x893976b8, &t.od.key.from }, { 0x893976bc, &t.od.key.cents }, { 0x89397e4c, &t.ws.gainScale },
        { 0x893976b0, &t.ws.k63 }, { 0x893976b4, &t.ws.key.scale }, { 0x893976b8, &t.ws.key.from },
        { 0x893976bc, &t.ws.key.cents }, { 0x89397920, &t.resonator.percent }, { 0x8939792c, &t.resonator.twelve },
        { 0x89397930, &t.resonator.sizeScale }, { 0x893976b0, &t.resonator.k63 }, { 0x89397934, &t.shifter.cents },
        { 0x89399500, &t.shifter.k63 }, { 0x8939793c, &t.lofi.key.scale }, { 0x89397940, &t.lofi.key.from },
        { 0x89397934, &t.lofi.key.cents }, { 0x89399500, &t.lofi.k63 }, { 0x893976c4, &t.sideBand.half },
        { 0x893976c8, &t.sideBand.percent }, { 0x893976cc, &t.sideBand.twelve }, { 0x893976d4, &t.sideBand.quarter },
        { 0x893976bc, &t.sideBand.widthCents }, { 0x893976b0, &t.sbf1.k63 }, { 0x893985e0, &t.sbf1.ownK63 },
        { 0x89398890, &t.sbf2.k63 }, { 0x89398890, &t.sbf2.ownK63 }, { 0x893985e4, &t.sbf2.qScale },
    });
}

struct Read : Vc2CosmTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, cosmWords(*this));
        for (int i = 0; i < kGains; ++i) gains[i] = (uint16_t)image.rd16(kGainTable + 2 * (uint32_t)i);
        readRoutines(image, kMonoRoutines, object.mono);
        readRoutines(image, kStereoRoutines, object.stereo);
        vc2ReadFloats(image, { kObjectFactors, Vc2CosmObjectPool::kFactors }, object.factors);
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadCosm(const char* path) { return tables.load(path); }
bool vc2CosmLoaded() { return tables.loaded(); }
const Vc2CosmTables& vc2CosmTables() { return tables.t; }
