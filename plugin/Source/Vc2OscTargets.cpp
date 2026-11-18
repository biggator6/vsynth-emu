// The OSC voice's targets: see Vc2OscTargets.h (v232).  Every address is rom.4's; every offset the voice's.
#include "Vc2OscTargets.h"
#include "XtFloat.h"

using xtf::add;
using xtf::mul;

namespace {
// [FW] the block's words the targets read (the voice's +0x54 on): the pitch, the ratio's scale, the walk's and the
// formant's bases, the first envelope's depth on the pitch, the gain's two factors, the vibrato's depths on the pitch
// and the gains
constexpr uint32_t kPitch = 0x5c, kRatioScale = 0x60, kWalkBase = 0x64, kFormantBase = 0x68, kPitchDepth = 0x70;
constexpr uint32_t kLevel = 0x88, kGain = 0xb0, kVibratoDepth = 0xfc, kLevelDepth = 0x100;
// [FW] the bindings (the tail's +0xa4c on: four envelopes, then four LFOs) the pitch and the level read, and the bound
// words: an envelope's output, its +0x1c and +0x2c, an LFO's output
constexpr size_t kPitchEnvelopeBind = 0, kLevelEnvelopeBind = 3;
constexpr uint32_t kEnvelopeOut = 0x4c, kEnvelopeHeld = 0x1c, kEnvelopeHeldOut = 0x2c, kLfoOut = 0x38;
// [FW] the walk's envelope 2 by +0x78 and LFO 2 by +0x98; the formant's envelope 3 by +0x80 and LFO 3 by +0xa0
constexpr Vc2OscTargets::Mod kWalkMod { 1, 0x78, 5, 0x98 }, kFormantMod { 2, 0x80, 6, 0xa0 };
// [FW] the walk's factor: the object's flags' 0x02 keeps it; else kind 2 with the block's +0x138 0x20, or kind 1 but
// for the wave's type 2, take 1
constexpr uint32_t kFlags = 0x15c, kFixedByte = 0x138;
constexpr uint8_t kKeepsFactor = 0x02, kFixedBit = 0x20;
constexpr Vc2OscBounds kUnit { 1.0f, -1.0f };   // [FW] 0x893aff62, 0x893affb0: fldi1, and its fneg
constexpr float kFormantMost = 2.0f;           // [FW] 0x893aff00: fldi1 added to itself

float bind(const Vc2OscVoice& v, Vc2OscParts& p, size_t which, uint32_t word) {
    return p.boundFloat(v.tail.binds[which], word);
}
} // namespace

float vc2OscBounded(float x, Vc2OscBounds b) {
    if (x > b.most) return b.most;
    if (b.least > x) return b.least;
    return x;
}

float vc2OscPitchSum(const Vc2OscVoice& v, Vc2OscParts& p) {
    float x = add(mul(bind(v, p, kPitchEnvelopeBind, kEnvelopeOut), v.f32(kPitchDepth)), v.f32(kPitch));
    x = add(mul(v.vibrato.w.pitch, v.f32(kVibratoDepth)), x);
    x = add(x, v.ramps[Vc2OscVoice::kPitchRamp].value());
    return add(x, v.tail.offset);
}

float Vc2OscTargets::cents() const { return vc2OscBounded(vc2OscPitchSum(v_, p_), vc2OscTables().cents); }

float Vc2OscTargets::ratioOn(const Vc2OscTargetPool& pool, Vc2OscBounds ratio) const {
    const float x = vc2OscBounded(vc2OscPitchSum(v_, p_), pool.cents);
    const float r = mul(mul(s_.exp2(mul(x, pool.perOctave)), v_.f32(kRatioScale)), v_.tail.rateRatio);
    return vc2OscBounded(r, ratio);
}

float Vc2OscTargets::ratio() const {
    const Vc2OscTables& t = vc2OscTables();
    return ratioOn({ t.cents, t.ratio.ratio, t.ratio.perOctave }, t.ratio.ratio);
}

float Vc2OscTargets::tunedRatio() const {
    const Vc2OscTables& t = vc2OscTables();
    const float r = ratioOn(t.ratio, t.ratio.ratio);
    return mul(r, s_.exp2(mul(-v_.tail.detune, t.ratio.perOctave)));
}

float Vc2OscTargets::modulation(const Mod& m) const {
    const float lfo = mul(bind(v_, p_, m.lfo, kLfoOut), v_.f32(m.lfoDepth));
    return add(mul(bind(v_, p_, m.envelope, kEnvelopeOut), v_.f32(m.envelopeDepth)), lfo);
}

float Vc2OscTargets::walkTarget() const {
    const float sum = modulation(kWalkMod);
    const float ramp = v_.ramps[Vc2OscVoice::kWalkRamp].value();
    return vc2OscBounded(mul(ramp, add(v_.f32(kWalkBase), sum)), vc2OscTables().walkTarget);
}

float Vc2OscTargets::formantTarget() const {
    const float sum = modulation(kFormantMod);
    const float ramp = v_.ramps[Vc2OscVoice::kFormantRamp].value();
    const float x = s_.exp2(add(v_.f32(kFormantBase), sum));
    return vc2OscBounded(mul(ramp, x), { kFormantMost, vc2OscTables().formantTargetLeast });
}

float Vc2OscTargets::walk() const {
    float factor = v_.tail.walkFactor;
    const bool keeps = (v_.u8(kFlags) & kKeepsFactor) != 0;
    if (!keeps && v_.kind() == 2 && (v_.u8(kFixedByte) & kFixedBit) != 0) factor = 1.0f;
    if (!keeps && v_.kind() == 1 && v_.u32(Vc2OscVoice::kWave) != 2) factor = 1.0f;
    const float sum = modulation(kWalkMod);
    const float ramp = v_.ramps[Vc2OscVoice::kWalkRamp].value();
    return mul(vc2OscBounded(mul(add(v_.f32(kWalkBase), sum), ramp), vc2OscTables().walk), factor);
}

float Vc2OscTargets::formant() const {
    const float sum = modulation(kFormantMod);
    const float ramp = v_.ramps[Vc2OscVoice::kFormantRamp].value();
    const float x = s_.exp2(add(v_.f32(kFormantBase), sum));
    const float r = vc2OscBounded(mul(x, ramp), { kFormantMost, vc2OscTables().formantLeast });
    return mul(v_.tail.formantFactor, r);
}

float Vc2OscTargets::levelOne() const { return add(mul(v_.vibrato.w.level, v_.f32(kLevelDepth)), 1.0f); }

float Vc2OscTargets::gain() const {
    const float g = mul(mul(levelOne(), v_.f32(kLevel)), v_.f32(kGain));
    return mul(g, v_.ramps[Vc2OscVoice::kPitchRamp].factor());
}

void Vc2OscTargets::writeInto(Vc2OscVoice& v, std::span<const Word> words) const {
    for (const auto& [at, target] : words) v.setF32(at, (this->*target)());
}

float Vc2OscTargets::level() const {
    const float one = levelOne();
    const bool held = bind(v_, p_, kLevelEnvelopeBind, kEnvelopeHeld) == 0.0f;
    const float envelope = bind(v_, p_, kLevelEnvelopeBind, held ? kEnvelopeOut : kEnvelopeHeldOut);
    const float l = vc2OscBounded(mul(one, mul(envelope, v_.f32(kLevel))), kUnit);
    return mul(v_.f32(kGain), l);
}
