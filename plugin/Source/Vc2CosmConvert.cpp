// The VC-2's COSM converters' frame: see Vc2CosmConvert.h (v235, v237).  Every address is rom.4's.
#include "Vc2CosmConvert.h"
#include "Vc2CosmTables.h"
#include "Vc2VocoderOsc.h"
#include "XtFloat.h"

using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::mul;

namespace {
constexpr int kTypes = 16;                                   // [FW] the switch's: 1…16
constexpr int kSpeaker = 4, kSpeakerCode = 5;                // [FW] its inline case: the code, and the byte +4
constexpr int kDepthMost = 63;                               // [FW] a depth's hold
constexpr int kLevelMost = 127, kKeyCentre = 60, kFollowScale = 10;   // [FW] a level's hold, the keyfollow's key and ×10

constexpr int kBalanceMost = 100;                            // [FW] a Dry/Effect Balance's hold and ÷

float factorOf(int velocity, int sens, int curve, const Vc2ToneShared& shared) {
    const auto f = static_cast<uint16_t>(shared.velocity(velocity, sens, curve));
    return div(flt(f), vc2CosmTables().velocityScale);
}

// The keyfollow's key and follow, each 16 bits: key − 60, follow × 10
int16_t keyOf(uint8_t key) { return static_cast<int16_t>(key - kKeyCentre); }
int16_t followOf(int16_t follow) { return static_cast<int16_t>(follow * kFollowScale); }
} // namespace

int vc2CosmLevel(int16_t param, int16_t sum, int most) {
    const auto v = static_cast<int16_t>(param + sum);
    return v > most ? most : v < 0 ? 0 : v;
}

float vc2CosmDepth(int16_t depth, float k63) {
    const float x = div(flt(depth), k63);
    return mul(depth < 0 ? -x : x, x);
}

float vc2CosmSummedDepth(int16_t depth, int16_t sum, float k63) {
    const int32_t d = depth + sum;
    return vc2CosmDepth(static_cast<int16_t>(d > kDepthMost ? kDepthMost : d < -kDepthMost ? -kDepthMost : d), k63);
}

int16_t Vc2CosmSums::operator()(int index) const { return matrix.sums(in.second != 0, in.matrix)[index]; }
int Vc2CosmSums::second() const { return in.second != 0 ? 1 : 0; }

float vc2CosmUnit(float x) { return add(div(x, flt(kLevelMost)), 0.0f); }

float vc2CosmFollowed(uint8_t key, int16_t follow, float percent, float by) {
    const int16_t k = keyOf(key), f = followOf(follow);
    return div(div(mul(flt(k), flt(f)), percent), by);
}

float vc2CosmKeyed(int level, uint8_t key, int16_t follow, float percent, float k127) {
    const float x = add(add(vc2CosmFollowed(key, follow, percent, k127), div(flt(level), flt(kLevelMost))), 0.0f);
    return x > 1.0f ? 1.0f : 0.0f > x ? 0.0f : x;
}

float vc2CosmKeyScale(uint8_t key, int16_t follow, float cents, const Vc2ToneShared& shared) {
    return shared.pow(2.0f, div(flt(keyOf(key) * followOf(follow)), cents));
}

float vc2CosmLevelKeyed(int level, uint8_t key, int16_t follow, const Vc2CosmLevelKeyPool& k,
                        const Vc2ToneShared& shared) {
    const float base = add(div(mul(flt(level), k.scale), flt(kLevelMost)), k.from);
    const float x = mul(base, vc2CosmKeyScale(key, follow, k.cents, shared));
    return x > 1.0f ? 1.0f : k.from > x ? k.from : x;
}

float vc2CosmScaled(int level, uint8_t key, int16_t follow, float cents, const Vc2ToneShared& shared) {
    const float x = mul(vc2CosmKeyScale(key, follow, cents, shared), vc2CosmUnit(flt(level)));
    return x > 1.0f ? 1.0f : 0.0f > x ? 0.0f : x;
}

float vc2CosmBalance(int16_t balance, int16_t sum) {
    const auto scaled = static_cast<int16_t>(sum * kBalanceMost / kLevelMost);
    return add(div(flt(vc2CosmLevel(balance, scaled, kBalanceMost)), flt(kBalanceMost)), 0.0f);
}

float vc2CosmQ(int q, float scale) { return add(div(mul(flt(q), scale), flt(kLevelMost)), -1.0f); }

float vc2CosmGain(int index, float scale) {
    const uint16_t* gains = vc2CosmTables().gains;
    return div(flt(index >= 0 && index < Vc2CosmTables::kGains ? gains[index] : 0), scale);
}

Vc2CosmMod vc2CosmModOf(int16_t env, float velocity, int32_t envelope, int16_t lfo, int16_t lfoSum, float k63) {
    return { mul(velocity, vc2CosmDepth(env, k63)), envelope, vc2CosmSummedDepth(lfo, lfoSum, k63), 0 };
}

void vc2CosmConvert(Vc2CosmInput& in, uint32_t* block, Vc2CosmTypes& types, const Vc2ToneShared& shared) {
    if (in.on == 0) {
        block[0] = 0;
        return;
    }
    in.velocity[0] = factorOf(in.noteVelocity, in.sensA, in.curveA, shared);
    in.velocity[1] = factorOf(in.noteVelocity, in.sensB, in.curveB, shared);
    if (static_cast<uint32_t>(in.type) - 1u >= static_cast<uint32_t>(kTypes)) {
        block[0] = 0;
    } else if (in.type == kSpeaker) {
        block[0] = kSpeakerCode;
        reinterpret_cast<uint8_t*>(block)[4] = static_cast<uint8_t>(in.params[0]);
    } else {
        types.convert(in.type, in, block);
    }
}
