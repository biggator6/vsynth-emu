// The players' frames' shared steps (v335, from mode 1's v327 and SOLO's v334, the period-jump player's too): a
// sample between two by a fraction, and the gain's glide toward its target, as each render's frame runs them in
// rom.4 (0x893ca080…, 0x893c97b0…, 0x893caa7a…); and SOLO's and the period-jump player's block on the walk.  Every
// offset the voice's.  Audio thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscWalk.h"
#include "XtFloat.h"

// [FW] SOLO's 0x893c9450, the period-jump player's 0x893ca8b0: the beat's block (0x893c89a0, Vc2OscVoice::beatBlock)
// gives the step's word; the player's head of the block; the next crossing (0x893c9380); then 32 frames, the player's
void vc2WalkBlock(Vc2OscVoice& v, float* out, Vc2OscParts& p, Vc2WalkHead head, Vc2WalkFrame frame) {
    constexpr int kFrames = 32;
    Vc2WalkWords w { v, p };
    v.beatBlock(p);
    head(w);
    vc2WalkCrossing(v, p);
    const Vc2OscSamples wave = p.waveSamples();
    for (int k = 0; k < kFrames; ++k) frame(w, wave, out + 2 * k);
}

// [FW] the step's half onto the fraction, its carry past 20 bits onto the position
int32_t vc2WalkHalfStep(int32_t at, int32_t& frac, int32_t step) {
    constexpr int kFracBits = 20;
    constexpr uint32_t kFracMask = 0xfffff;
    const int32_t sum = vc2WalkPlus(frac, step >> 1);
    frac = (int32_t)((uint32_t)sum & kFracMask);
    return vc2WalkPlus(at, sum >> kFracBits);
}

float vc2OscBetween(int a, int b, float f) {
    const float s = xtf::flt(a);
    return xtf::add(xtf::mul(xtf::sub(xtf::flt(b), s), f), s);
}

float vc2OscGlide(Vc2OscVoice& v, uint32_t target, float scale) {
    constexpr uint32_t kGain = 4, kRate = 8;   // [FW] past the target
    const float gain = v.f32(target + kGain);
    const float g = xtf::add(xtf::mul(v.f32(target + kRate), xtf::sub(v.f32(target), gain)), gain);
    v.setF32(target + kGain, g);
    return xtf::mul(g, scale);
}
