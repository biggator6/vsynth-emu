// COSM type 8, the VC-2's GROWL, as read from rom.4 (vc2cosmgrowllaw.h).
#include "vc2cosmgrowllaw.h"
#include <algorithm>
#include <cmath>

namespace vc2 {

namespace {

constexpr uint32_t kDspPool = 0x893b9c80, kUpdatePool = 0x893b9e94;   // [FW] the DSP's and the update's constants

} // namespace

CosmGrowlConsts cosmGrowlConstsOf(Machine& m) {
    const auto f = [&m](uint32_t at) { return m.f32(at); };
    return {f(kDspPool), f(kDspPool + 4), f(kDspPool + 8), f(kDspPool + 12), f(kDspPool + 16), f(kDspPool + 20),
            f(kDspPool + 24), f(kDspPool + 28), f(kDspPool + 32), f(kDspPool + 36), f(kUpdatePool),
            f(kUpdatePool + 4), f(kUpdatePool + 8)};
}

float CosmGrowl::blockIntensity(int intensity) {
    return (float)std::clamp(intensity, 0, 127) / 127.0f + 0.0f;
}

float CosmGrowl::blockSpeed(int speed) {
    return (float)std::clamp(speed, 0, 128) * 2.0f / 128.0f + -1.0f;
}

void CosmGrowl::update(float intensity, float speed) {
    depth = std::clamp(intensity, 0.0f, 1.0f);
    factor = c.k440 / ((std::clamp(speed, c.speedFloor, 1.0f) + 1.0f) * c.k50);
}

float CosmGrowl::wave(float x) const {
    return (c.three - x * x * c.four) * x;
}

void CosmGrowl::block(float pitch, float* io) {
    const float fm = depth * c.fmScale / pitch;
    if (c.tiny > step) phase = step = 1.0f;
    for (int k = 0; k < kGrowlFrames; ++k, io += 2) {
        // the growl's phase, −1…1, its step set at each wrap: the pitch over a divider n near the factor × the pitch
        phase = phase + step;
        if (!(1.0f > phase)) {
            int n = std::max((int)(factor * pitch), 2);
            phase = phase + c.minusTwo;
            n += !plant && (pos & 1) ? 1 : 0;
            step = pitch * c.rate / (float)n;
        }
        pos = pos + 1 >= kGrowlLine ? 0 : pos + 1;
        l[pos] = io[0];
        r[pos] = io[1];
        // the first tap, 512 back, moved in level by the wave of 0.5 − |phase|
        const int tap = pos - kGrowlTapBack < 0 ? pos - kGrowlTapBack + kGrowlLine : pos - kGrowlTapBack;
        const float gain = fmac(wave(c.half - std::fabs(phase)), depth, 1.0f);
        const float amL = l[tap] * gain, amR = r[tap] * gain;
        // the second, moved in position by the wave of the phase folded to ±0.5, read between the sample before and it
        float x = phase;
        if (phase > c.half) x = 1.0f - phase;
        else if (c.minusHalf > phase) x = -1.0f - phase;
        float at = fmac(wave(x), fm, (float)tap);
        if (!(c.line > at)) at = at + c.minusLine;
        else if (0.0f > at) at = at + c.line;
        const int i = (int)at, j = i - 1 < 0 ? i - 1 + kGrowlLine : i - 1;
        const float frac = at - (float)i;
        io[0] = fmac(l[i] - l[j], frac, l[j]) + amL;
        io[1] = fmac(r[i] - r[j], frac, r[j]) + amR;
    }
}

} // namespace vc2
