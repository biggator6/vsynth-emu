// The VC-2's COSM converters' frame: see Vc2CosmConvert.h (v235).  Every address is rom.4's.
#include "Vc2CosmConvert.h"
#include "Vc2CosmTables.h"
#include "Vc2VocoderOsc.h"
#include "XtFloat.h"

using xtf::div;
using xtf::flt;
using xtf::mul;

namespace {
constexpr int kTypes = 16;                                   // [FW] the switch's: 1…16
constexpr int kSpeaker = 4, kSpeakerCode = 5;                // [FW] its inline case: the code, and the byte +4
constexpr int kDepthMost = 63;                               // [FW] a depth's hold

float factorOf(int velocity, int sens, int curve, const Vc2ToneShared& shared) {
    const auto f = static_cast<uint16_t>(shared.velocity(velocity, sens, curve));
    return div(flt(f), vc2CosmTables().velocityScale);
}
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
