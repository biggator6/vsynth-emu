// The VC-2's COSM constants from rom.4: see Vc2CosmTables.h (v234, v235, v237).  Every address is rom.4's, each the pool
// word its routine loads.
#include "Vc2CosmTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr uint32_t kGainTable = 0x89541f34;   // [FW] the converters' gain table, u16 each

// [FW] the Growl's DSP's pool and its update's; the TVF's converter's and the dispatcher's; the batch of v237's
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
    });
}

struct Read : Vc2CosmTables {
    bool read(Sh4& image) {
        vc2ReadPool(image, cosmWords(*this));
        for (int i = 0; i < kGains; ++i) gains[i] = (uint16_t)image.rd16(kGainTable + 2 * (uint32_t)i);
        return true;
    }
};

constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadCosm(const char* path) { return tables.load(path); }
bool vc2CosmLoaded() { return tables.loaded(); }
const Vc2CosmTables& vc2CosmTables() { return tables.t; }
