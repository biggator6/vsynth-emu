// rom.4's COSM object's frame: see Vc2CosmObject.h (v249).  Every address is rom.4's.
#include "Vc2CosmObject.h"
#include "Vc2CosmTables.h"

namespace {
constexpr uint32_t kVtable = 0x898cd478;   // [FW] the COSM object's vtable
constexpr int kLastCode = 20;              // [FW] the install's switch
constexpr int8_t kStereoOnMono = 10;       // [FW] the code a mono install hands on as stereo

// How a code's float is chosen: its own, or by the block's variant byte (the Wave Shaper's Type 3 and up, the Amp's
// Type), or by the TVF's Filter Type in fives
enum class Vc2CosmFactorKind { Own, Curve, Type, Fives };
using enum Vc2CosmFactorKind;

// [FW] each code's first float and how it moves on: 0 Through, 3 the Wave Shaper, 4 the Amp Simulator, 13 the TVF, 14
// the Dynamic TVF, the rest one each
struct Vc2CosmObjectCase {
    int index;
    Vc2CosmFactorKind kind;
};
constexpr Vc2CosmObjectCase kCases[kLastCode + 1] { { 0, Own }, { 1, Own }, { 2, Own }, { 3, Curve }, { 5, Type },
    { 8, Own }, { 9, Own }, { 10, Own }, { 11, Own }, { 12, Own }, { 13, Own }, { 14, Own }, { 15, Own }, { 16, Fives },
    { 19, Fives }, { 22, Own }, { 23, Own }, { 24, Own }, { 25, Own }, { 26, Own }, { 27, Own } };
constexpr uint8_t kCurveStep = 3;          // [FW] the Wave Shaper's: Type 3 and up the next float
constexpr int32_t kFives = 5;              // [FW] the TVF's Filter Type in fives (× 0x66666667, then >> 33)

int caseIndex(const Vc2CosmObjectWords& o, const Vc2CosmObjectCase& c) {
    switch (c.kind) {
    case Own: return c.index;
    case Curve: return c.index + (o.variant >= kCurveStep ? 1 : 0);
    case Type: return c.index + o.variant;
    case Fives: return c.index + o.filterType / kFives;
    }
    return c.index;
}
} // namespace

void vc2CosmConstruct(Vc2CosmObjectWords& o) {
    o.vtable = kVtable;
    o.factor = 0.0f;
}

uint32_t vc2CosmVtable() { return kVtable; }

void vc2CosmSetUp(Vc2CosmObjectWords& o, const Vc2CosmSetUpWords& s, uint32_t flag) {
    o.setUp = s;
    o.setUp.flag = flag != 0 ? 1 : 0;
}

void vc2CosmBind(Vc2CosmObjectWords& o, const std::array<uint32_t, 4>& bindings) { o.bindings = bindings; }

uint32_t vc2CosmInstall(Vc2CosmObjectWords& o, uint32_t flag) {
    const Vc2CosmObjectPool& t = vc2CosmTables().object;
    const bool stereo = flag != 0;
    const auto code = static_cast<int8_t>(o.dspCode);   // mov.b, signed
    o.routine = (stereo ? t.stereo : t.mono)[code + Vc2CosmObjectPool::kCodeReach];
    if (code >= 0 && code <= kLastCode) {
        const int index = (stereo ? Vc2CosmObjectPool::kTypes : 0) + caseIndex(o, kCases[code]);
        // Past the floats no converter's variant reaches (a Filter Type of 1,210 and up); rom.4 reads on
        if (index >= 0 && index < Vc2CosmObjectPool::kFactors) o.factor = t.factors[index];
    }
    return stereo ? 1 : code == kStereoOnMono;
}
