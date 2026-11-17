// The VC-2's COSM type 8, the GROWL: see Vc2CosmGrowl.h (v234).  Every address is rom.4's; each routine in its order of
// operations, single precision under the SH-4's rounding (XtFloat), FMAC as a product then a sum.
#include "Vc2CosmGrowl.h"
#include "Vc2CosmConvert.h"
#include "XtFloat.h"
#include <cstring>

using xtf::add;
using xtf::div;
using xtf::flt;
using xtf::ftrc;
using xtf::mul;
using xtf::sub;

namespace {
constexpr int kIntensityMax = 127, kSpeedMax = 128;   // [FW] the converter's holds, and its divisors
constexpr int32_t kLeastDivider = 2;                  // [FW] the DSP's least divider
constexpr uint32_t kCopied = sizeof(Vc2CosmGrowlBlock) - sizeof(int32_t);   // [FW] the new block's ten words from +4

float fmac(float a, float b, float c) { return add(mul(a, b), c); }
} // namespace

float vc2CosmClamp(float x, float lo, float hi) {
    if (lo > x) x = lo;
    if (x > hi) x = hi;
    return x;
}

void vc2GrowlConvert(const Vc2CosmInput& in, Vc2CosmGrowlBlock& b, Vc2CosmMatrix& matrix) {
    b.code = Vc2CosmGrowl::kCode;
    const float intensity = flt(vc2CosmLevel(in.params[0], matrix.sums(in.second != 0, in.matrix)[0], kIntensityMax));
    b.intensity = add(div(intensity, flt(kIntensityMax)), 0.0f);
    b.intensityMod = Vc2CosmMod {};
    const float speed = flt(vc2CosmLevel(in.params[1], matrix.sums(in.second != 0, in.matrix)[1], kSpeedMax));
    b.speed = add(div(mul(speed, 2.0f), flt(kSpeedMax)), -1.0f);
    b.speedMod = Vc2CosmMod {};
}

Vc2CosmGrowl::Vc2CosmGrowl() : left(kLine, 0.0f), right(kLine, 0.0f) {}

void Vc2CosmGrowl::noteOn() {
    w.phase = 1.0f;
    w.step = 1.0f;
}

void Vc2CosmGrowl::update() {
    const Vc2CosmGrowlPool& k = vc2CosmTables().growl;
    const Vc2CosmGrowlBlock& b = w.block;
    const Vc2CosmMod &i = b.intensityMod, &s = b.speedMod;
    w.depth = vc2CosmClamp(fmac(i.lfo, w.lfoA, fmac(i.env, w.envA, b.intensity)), 0.0f, 1.0f);
    const float speed = fmac(s.lfo, w.lfoB, fmac(s.env, w.envB, b.speed));
    w.factor = div(k.k440, mul(add(vc2CosmClamp(speed, k.speedFloor, 1.0f), 1.0f), k.k50));
}

void Vc2CosmGrowl::newBlock(const Vc2CosmGrowlBlock& b) {
    auto* to = reinterpret_cast<char*>(&w.block) + sizeof(int32_t);
    std::memcpy(to, reinterpret_cast<const char*>(&b) + sizeof(int32_t), kCopied);
    update();
}

// A line's sample; outside the line, where rom.4 reads whatever lies there (a pitch under depth × 15.95 / 3267, about
// 7.7 octaves down, swings the second tap past one wrap), 0
float Vc2CosmGrowl::line(const std::vector<float>& l, int32_t i) const {
    return i >= 0 && i < kLine ? l[(size_t)i] : 0.0f;
}

void Vc2CosmGrowl::render(float* frames) {
    const Vc2CosmGrowlPool& k = vc2CosmTables().growl;
    const float swing = div(mul(w.depth, k.fmScale), w.pitch);   // the second tap's, in samples
    float phase = w.phase, step = w.step;   // [FW] single precision, as rom.4 keeps them
    int32_t pos = w.position;
    if (k.tiny > step) phase = step = 1.0f;
    for (int f = 0; f < kFrames; ++f, frames += 2) {
        // the phase, −1…1: at each wrap its step is the pitch over a divider near the factor × the pitch, one more
        // at an odd position
        phase = add(phase, step);
        if (!(1.0f > phase)) {
            int32_t n = ftrc(mul(w.factor, w.pitch));
            phase = add(phase, k.minusTwo);
            if (n < kLeastDivider) n = kLeastDivider;
            if (pos & 1) n = static_cast<int32_t>(static_cast<uint32_t>(n) + 1u);
            step = div(mul(w.pitch, k.rate), flt(n));
        }
        pos = pos + 1 >= kLine ? 0 : pos + 1;
        left[(size_t)pos] = frames[0];
        right[(size_t)pos] = frames[1];
        // the first tap, 512 back, moved in level by the cosine of the phase
        const int32_t tap = pos - kTapBack < 0 ? pos - kTapBack + kLine : pos - kTapBack;
        const float gain = fmac(vc2Cosine(k.taps, phase), w.depth, 1.0f);
        const float levelL = mul(left[(size_t)tap], gain), levelR = mul(right[(size_t)tap], gain);
        // the second, moved in position by its sine, read between the sample before and the one at it
        float at = fmac(vc2Sine(k.taps, phase), swing, flt(tap));
        if (!(k.line > at)) {
            at = add(at, k.minusLine);
        } else if (0.0f > at) {
            at = add(at, k.line);
        }
        const int32_t i = ftrc(at), j = i - 1 < 0 ? i - 1 + kLine : i - 1;
        const float frac = sub(at, flt(i));
        const float l = line(left, j), r = line(right, j);
        frames[0] = add(fmac(sub(line(left, i), l), frac, l), levelL);
        frames[1] = add(fmac(sub(line(right, i), r), frac, r), levelR);
    }
    w.position = pos;
    w.phase = phase;
    w.step = step;
}
