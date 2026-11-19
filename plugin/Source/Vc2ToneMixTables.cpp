// The tone's OSC mix's constants from rom.4: see Vc2ToneMixTables.h (v269).  Every address is rom.4's.
#include "Vc2ToneMixTables.h"
#include "Vc2RomTables.h"

namespace {
constexpr uint32_t kGainsAt = 0x89542394;   // [FW] the install's table of output gains

// [FW] the gains, then the pool words the note-on, the renders and the follower load
auto toneMixPoolWords(Vc2ToneMixTables& t) {
    return std::to_array<Vc2PoolWord>({
        { kGainsAt, &t.gains[0] }, { kGainsAt + 4, &t.gains[1] }, { kGainsAt + 8, &t.gains[2] },
        { kGainsAt + 12, &t.gains[3] }, { kGainsAt + 16, &t.gains[4] }, { kGainsAt + 20, &t.gains[5] },
        { kGainsAt + 24, &t.gains[6] }, { kGainsAt + 28, &t.gains[7] },
        { 0x893e4670, &t.settle }, { 0x893e46f0, &t.ramp }, { 0x893bf180, &t.follower },
        { 0x893e4318, &t.rateSpan }, { 0x893e431c, &t.rateFloor }, { 0x893adc6c, &t.half },
        { 0x893add74, &t.fullScale }, { 0x893e4988, &t.delaySpan }, { 0x893adea8, &t.settingsScale },
    });
}

using Tables = Vc2PoolTables<Vc2ToneMixTables, toneMixPoolWords>;
} // namespace

bool vc2LoadToneMix(const char* path) { return Tables::load(path); }
bool vc2ToneMixLoaded() { return Tables::loaded(); }
const Vc2ToneMixTables& vc2ToneMixTables() { return Tables::get(); }
