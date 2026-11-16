// The VC-2's part mixer's block, 0x893b1820: see Vc2PartMixer.h (v227).  Its arithmetic in rom.4's order, in XtFloat;
// every address is rom.4's.
#include "Vc2PartMixer.h"
#include "Vc2PartMixerTables.h"
#include "XtFloat.h"
#include <bit>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;

constexpr int32_t kInfinity = 0x7f800000;   // [FW] 0x893b1958: a word above it as an int, a positive NaN, is cleared
constexpr int kMicWord = 1, kSecondWord = 5;   // [FW] an input frame's +4 and +0x14 words

// A positive NaN cleared, as the mixer's integer compare finds it
void cleared(float& x) {
    if (std::bit_cast<int32_t>(x) > kInfinity) x = 0.0f;
}
} // namespace

void Vc2PartMixer::block(const int32_t* in, float* out, std::span<Vc2MixerVoice* const> voices, Vc2MixerParts& parts) {
    const Vc2PartMixerTables& t = vc2MixerTables();
    ++w.blocks;
    for (int i = 0; i < kBlock; ++i) {   // the left of the other buffer is not cleared
        b.carrier[i].left = b.carrier[i].right = 0.0f;
        b.direct[i].right = 0.0f;
        b.mic[i] = mul(xtf::flt(in[kFrameWords * i + kMicWord]), t.toFloat);
        b.second[i] = mul(xtf::flt(in[kFrameWords * i + kSecondWord]), t.toFloat);
    }
    parts.inputStage(b.second);
    parts.micRing(b.mic);
    if (!parts.engine()) parts.unvoiced(b.mic);   // with an engine, the vocoder's driver runs it on the ring
    w.carried = 0.0f;
    const bool extin = w.extin != 0;   // the direct signal: the mic itself, or the other buffer's right channel
    walk(voices, !extin);
    float* ring = parts.detectorRing();
    for (int i = 0; i < kBlock; ++i) {
        ring[i] = extin ? b.mic[i] : mul(b.direct[i].right, t.toRing);
        cleared(b.carrier[i].left);
        cleared(b.carrier[i].right);
        cleared(ring[i]);
    }
    if (bypass == 0) parts.vocoder(ring, b.carrier);
    if (extin) {
        frames(b.mic, 1, out, t.ease[1]);
    } else {
        frames(&b.direct[0].right, 2, out, t.ease[0]);
    }
}

void Vc2PartMixer::walk(std::span<Vc2MixerVoice* const> voices, bool askHeld) {
    for (Vc2MixerVoice* voice : voices) {
        if (voice->carrier()) {
            voice->renderCarrier(b.carrier);
            w.carried = 1.0f;
        } else if (!askHeld || !voice->held()) {
            voice->render(b.direct);
        }
    }
}

void Vc2PartMixer::frames(const float* direct, int stride, float* out, float ease) {
    const float stepMulti = mul(sub(b.multiTarget, b.multi), ease);
    const float stepReverb = mul(sub(b.reverbTarget, b.reverb), ease);
    const float stepMain = mul(sub(b.mainTarget, b.main), ease);
    for (int i = 0; i < kBlock; ++i) {
        const float d = direct[stride * i];
        float* f = out + kFrameWords * i;
        f[0] = mul(d, b.main);   // MAIN L and R
        f[1] = mul(d, b.main);
        b.main = add(b.main, stepMain);
        f[2] = mul(d, b.reverb);   // the Reverb send
        b.reverb = add(b.reverb, stepReverb);
        f[3] = 0.0f;               // the Chorus send
        f[4] = mul(b.carrier[i].left, b.multi);   // MULTI L and R
        f[5] = mul(b.carrier[i].right, b.multi);
        b.multi = add(b.multi, stepMulti);
    }
}
