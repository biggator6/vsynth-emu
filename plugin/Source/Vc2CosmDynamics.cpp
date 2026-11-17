// The VC-2's COSM Amp Simulator's, Compressor's and Limiter's converters: see Vc2CosmDynamics.h (v237).  In rom.4's
// order of operations, single precision under the SH-4's rounding (XtFloat), FMAC as a product then a sum.
#include "Vc2CosmDynamics.h"
#include "Vc2CosmTables.h"
#include "XtFloat.h"

using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;

namespace {
constexpr int kMost = 127, kOver = 100;                     // [FW] the levels' hold; the level's gain's knee
constexpr int32_t kAmpCode = 4, kCompCode = 15, kLimCode = 16;   // [FW]
// [FW] the Amp Simulator's parameters: Amp Type, Bass, Middle, Treble, Output Level, Gain, its LFO and Env depths; its
// sums (the group's 2nd, 1st and 10th) and Gain's envelopes
constexpr int kAmpType = 0, kBass = 1, kMiddle = 2, kTreble = 3, kAmpLevel = 4, kGain = 5, kGainLfo = 6, kGainEnv = 7;
constexpr int kTrebleSum = 1, kGainSum = 0, kGainLfoSum = 9;
constexpr int32_t kGainEnvelope[2] = { 9, 11 };
// [FW] the Limiter's: Threshold Level, Ratio, Attack, Release, Output Level; the Compressor's Attack, Sustain, Level
constexpr int kThreshold = 0, kRatio = 1, kLimAttack = 2, kRelease = 3, kLimLevel = 4;
constexpr int kAttack = 0, kSustain = 1, kCompLevel = 2;

// 0x89397900, the level's gain: (L/100)² up to 100, 1 + (L − 100)/27 × 3 above
float levelGain(int16_t level) {
    const Vc2CosmLevelGainPool& k = vc2CosmTables().levelGain;
    if (level > kOver) return add(mul(div(flt(level - kOver), k.over), k.overGain), 1.0f);
    const float g = div(flt(level), k.percent);
    return mul(g, g);
}
} // namespace

void vc2AmpConvert(const Vc2CosmInput& in, Vc2CosmAmpBlock& b, Vc2CosmMatrix& matrix) {
    const Vc2CosmAmpPool& k = vc2CosmTables().amp;
    const Vc2CosmSums sum { in, matrix };
    b.code = kAmpCode;
    b.ampType = static_cast<uint8_t>(in.params[kAmpType]);
    b.bass = vc2CosmUnit(mul(flt(vc2CosmLevel(in.params[kBass], 0, kMost)), k.bass));
    b.middle = vc2CosmUnit(flt(vc2CosmLevel(in.params[kMiddle], 0, kMost)));
    b.treble = vc2CosmUnit(mul(flt(vc2CosmLevel(in.params[kTreble], sum(kTrebleSum), kMost)), 2.0f));
    b.level = vc2CosmGain(in.params[kAmpLevel], k.gainScale);
    b.gain = vc2CosmGain(vc2CosmLevel(in.params[kGain], sum(kGainSum), kMost), k.gainScale);
    b.gainMod = vc2CosmModOf(in.params[kGainEnv], in.velocity[0], kGainEnvelope[sum.second()], in.params[kGainLfo],
                             sum(kGainLfoSum), k.k63);
}

void vc2CompConvert(const Vc2CosmInput& in, Vc2CosmCompBlock& b, Vc2CosmMatrix& matrix) {
    const Vc2CosmSums sum { in, matrix };
    b.code = kCompCode;
    b.attack = vc2CosmUnit(flt(vc2CosmLevel(in.params[kAttack], sum(0), kMost)));
    b.sustain = vc2CosmUnit(flt(vc2CosmLevel(in.params[kSustain], sum(1), kMost)));
    b.gain = levelGain(in.params[kCompLevel]);
}

void vc2LimConvert(const Vc2CosmInput& in, Vc2CosmLimBlock& b, Vc2CosmMatrix& matrix) {
    const float scale = vc2CosmTables().limiterScale;
    const Vc2CosmSums sum { in, matrix };
    b.code = kLimCode;
    b.threshold = vc2CosmGain(in.params[kThreshold], scale);
    b.ratio = static_cast<uint8_t>(in.params[kRatio]);
    b.attack = vc2CosmGain(vc2CosmLevel(in.params[kLimAttack], sum(0), kMost), scale);
    b.release = vc2CosmGain(vc2CosmLevel(in.params[kRelease], sum(1), kMost), scale);
    b.gain = levelGain(in.params[kLimLevel]);
}
