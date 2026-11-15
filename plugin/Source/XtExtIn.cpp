// The EXT IN OSC's player and its input: see XtExtIn.h (v203).
#include "XtExtIn.h"
#include "XtFloat.h"

#include <algorithm>

namespace {
constexpr float kGlide = 0x1p-5f;   // [FW 0x89509830] 1/32: the share of the gap to the target the gain goes a sample
constexpr int kBlockFrames = 32;
}

void XtExtInPlayer::noteOn(float start, float target) {
    gain_ = start;
    target_ = target;
}

// [FW 0x895097d0] each frame: L·gain and R·gain out, then the gain by FMAC (the product rounded, then the sum)
void XtExtInPlayer::block(float* lr, const int32_t* in) {
    using namespace xtf;
    for (int i = 0; i < kBlockFrames; ++i) {
        lr[2 * i] = mul(flt(in[2 * i]), gain_);
        lr[2 * i + 1] = mul(flt(in[2 * i + 1]), gain_);
        gain_ = add(mul(sub(target_, gain_), kGlide), gain_);
    }
}

void XtExtInput::push(const int32_t* lr, int frames) {
    for (int i = 0; i < frames; ++i, ++write_) {
        ring_[2 * (write_ % kFrames)] = lr[2 * i];
        ring_[2 * (write_ % kFrames) + 1] = lr[2 * i + 1];
    }
    if (write_ - read_ > kFrames) read_ = write_ - kFrames;   // a ring overrun keeps the newest
}

const int32_t* XtExtInput::block() { return take(kBlockFrames); }

const int32_t* XtExtInput::take(int frames) {
    out_.fill(0);
    const uint32_t n = std::min<uint32_t>(write_ - read_, (uint32_t)std::clamp(frames, 0, kBlockFrames));
    for (uint32_t i = 0; i < n; ++i, ++read_) {
        out_[2 * i] = ring_[2 * (read_ % kFrames)];
        out_[2 * i + 1] = ring_[2 * (read_ % kFrames) + 1];
    }
    return out_.data();
}

void XtExtInput::clear() {
    read_ = write_;
    out_.fill(0);
}

void XtExtInCopy::setType(int type) { type_ = std::clamp(type, 0, 3); }

// [FW program A, measured on XtFxChip] each Type's pair, from the frame before: STEREO L R the sides, MONO MIX each side
// halved on its own and summed (the routing's 0x20 terms, at shift code 1), MONO L the left on both, MONO R the right
int32_t XtExtInCopy::wordOf(int side) const {
    const int32_t l = last_[0], r = last_[1], mix = (l >> 1) + (r >> 1);
    const int32_t laws[4][2] = { { l, r }, { mix, mix }, { l, l }, { r, r } };
    return (int32_t)((uint32_t)laws[type_][side] << 8);
}

void XtExtInCopy::into(XtExtInput& to, const int32_t* lr, int frames) {
    for (int i = 0; i < frames; ++i) {
        const int32_t back[2] = { wordOf(0), wordOf(1) };
        to.push(back, 1);
        last_[0] = lr[2 * i] >> 8;   // the chip has a word's top 24 bits, as XtFxUnit gives it
        last_[1] = lr[2 * i + 1] >> 8;
    }
}

#ifdef XT_TONE_CHECK
void xtExtInTestBlock(long from, int32_t* lr64) {
    for (int i = 0; i < 2 * kBlockFrames; ++i) {
        uint64_t z = (uint64_t)(2 * from + i) + 0x9e3779b97f4a7c15ull;   // splitmix64 of the frame and side
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        lr64[i] = (int32_t)(uint32_t)(z ^ (z >> 31));
    }
}
#endif
