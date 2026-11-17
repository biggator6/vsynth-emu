// The VC-2's COSM Resonator's, Frequency Shifter's and Lo-Fi's converters: see Vc2CosmBalanced.h (v239).  In rom.4's
// order of operations, single precision under the SH-4's rounding (XtFloat).
#include "Vc2CosmBalanced.h"
#include "Vc2CosmTables.h"
#include "Vc2VocoderOsc.h"
#include "XtFloat.h"

using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;

namespace {
constexpr int kMost = 127;                                  // [FW] the settings' hold
constexpr int32_t kResonatorCode = 6, kShifterCode = 18, kLofiCode = 19;   // [FW]
constexpr int32_t kSettingEnvelope[2] = { 9, 11 }, kBalanceEnvelope[2] = { 10, 12 };   // [FW] COSM 1's, COSM 2's
// [FW] the sums: the group's 1st, 10th, 2nd and 11th
constexpr int kSettingSum = 0, kSettingLfoSum = 9, kBalanceSum = 1, kBalanceLfoSum = 10;

// [FW] a converter's parameters in its input: its setting, the setting's Keyfollow, LFO and Env depths, then the
// Balance and its LFO and Env depths
struct Vc2CosmMixRow {
    int setting, follow, lfo, env, balance, balanceLfo, balanceEnv;
};
constexpr Vc2CosmMixRow kResonator { 1, 2, 3, 4, 5, 6, 7 };
constexpr Vc2CosmMixRow kShifter { 0, 1, 2, 3, 4, 5, 6 };
constexpr Vc2CosmMixRow kLofi { 2, 3, 4, 5, 6, 7, 8 };

// The words by a row, the setting by `law(held level, its keyfollow)`, the sums in rom.4's order
template <class Law>
void mixWords(const Vc2CosmMixRow& r, const Vc2CosmInput& in, Vc2CosmMatrix& matrix, float k63, Vc2CosmMixWords& w,
              Law law) {
    const Vc2CosmSums sum { in, matrix };
    w.setting = law(vc2CosmLevel(in.params[r.setting], sum(kSettingSum), kMost), in.params[r.follow]);
    w.settingMod = vc2CosmModOf(in.params[r.env], in.velocity[0], kSettingEnvelope[sum.second()], in.params[r.lfo],
                                sum(kSettingLfoSum), k63);
    w.balance = vc2CosmBalance(in.params[r.balance], sum(kBalanceSum));
    w.balanceMod = vc2CosmModOf(in.params[r.balanceEnv], in.velocity[1], kBalanceEnvelope[sum.second()],
                                in.params[r.balanceLfo], sum(kBalanceLfoSum), k63);
}

// 0x89397760, the Resonator's Size: the keyfollow's offset ÷ 100 ÷ 12 × logf(2) ÷ 1.6, plus the held Size × 2 ÷ 127,
// less 1, held to ±1
float resonatorSize(int size, uint8_t key, int16_t follow, const Vc2CosmResonatorPool& k,
                    const Vc2ToneShared& shared) {
    const float keyed = div(mul(vc2CosmFollowed(key, follow, k.percent, k.twelve), shared.log(2.0f)), k.sizeScale);
    const float x = add(add(keyed, div(mul(flt(size), 2.0f), flt(kMost))), -1.0f);
    return x > 1.0f ? 1.0f : -1.0f > x ? -1.0f : x;
}
} // namespace

void vc2ResonatorConvert(const Vc2CosmInput& in, Vc2CosmResonatorBlock& b, Vc2CosmMatrix& matrix,
                         const Vc2ToneShared& shared) {
    const Vc2CosmResonatorPool& k = vc2CosmTables().resonator;
    // one-off: a block's code and first byte are its own layout's words, written where rom.4 writes them
    b.code = kResonatorCode;
    b.filterType = static_cast<uint8_t>(in.params[0]);
    mixWords(kResonator, in, matrix, k.k63, b.mix,
             [&](int size, int16_t follow) { return resonatorSize(size, in.noteKey, follow, k, shared); });
}

void vc2ShifterConvert(const Vc2CosmInput& in, Vc2CosmShifterBlock& b, Vc2CosmMatrix& matrix,
                       const Vc2ToneShared& shared) {
    const Vc2CosmShifterPool& k = vc2CosmTables().shifter;
    b.code = kShifterCode;
    mixWords(kShifter, in, matrix, k.k63, b.mix,
             [&](int depth, int16_t follow) { return vc2CosmScaled(depth, in.noteKey, follow, k.cents, shared); });
}

void vc2LofiConvert(const Vc2CosmInput& in, Vc2CosmLofiBlock& b, Vc2CosmMatrix& matrix, const Vc2ToneShared& shared) {
    const Vc2CosmLofiPool& k = vc2CosmTables().lofi;
    b.code = kLofiCode;
    b.bitDown = static_cast<uint8_t>(in.params[0]);
    b.rateDown = static_cast<uint8_t>(in.params[1]);
    mixWords(kLofi, in, matrix, k.k63, b.mix,
             [&](int filter, int16_t follow) { return vc2CosmLevelKeyed(filter, in.noteKey, follow, k.key, shared); });
}
