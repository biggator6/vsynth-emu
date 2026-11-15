// The VC-2's consonants, both vocoder engines' output stage: see Vc2Unvoiced.h (v209).  rom.4's 0x893ed510 (mono) and
// 0x893ed690 (stereo) are one routine but for the last store, so they are one here by the channel count.  Every address
// is rom.4's; the check is xt_vc2in's (Vc2UnvoicedCheck.cpp).  An FMAC is the product rounded, then the sum (XtFloat).
#include "Vc2Unvoiced.h"
#include "XtFloat.h"
#include <bit>

namespace {
// [FW 0x893ed738…0x893ed774, the stereo routine's own copies at 0x893ed8a0…0x893ed8b8 the same] rom.4's bit patterns,
// some a unit off the nearest float
constexpr float kRise = std::bit_cast<float>(0x3b03126fu);          // 0.002: the gate toward a higher target
constexpr float kFall = std::bit_cast<float>(0x3c23d709u);          // 0.01: toward a lower one
constexpr float kGlide = 0.03125f;                                   // the level's, a block
constexpr float kFeed = std::bit_cast<float>(0x3eb33332u);          // 0.35: the highpass's
constexpr float kBack = std::bit_cast<float>(0x3fd33332u);          // 1.65: the same
constexpr float kAllpassGain = std::bit_cast<float>(0x3f199999u);   // 0.6
constexpr int kMicLate = 448, kDelayA = 280, kDelayB = 162;         // samples

// One allpass on its doubled buffer, as rom.4 orders it: w = (0 − g·d) + x, written at the index and 50,000 on; out =
// w·g + d, d read kDelay on from the index, which falls
float allpass(std::vector<float>& buffer, int32_t at, int delay, float x) {
    using namespace xtf;
    const float d = buffer[(size_t)(at + delay)];
    const float w = add(sub(0.0f, mul(d, kAllpassGain)), x);
    buffer[(size_t)(at + Vc2Unvoiced::kAllpass)] = buffer[(size_t)at] = w;
    return add(mul(w, kAllpassGain), d);
}
} // namespace

// A sample of the late mic: the gate glides toward the target, gates it, and the two-state highpass runs on that
float Vc2Unvoiced::filtered(float x, float target, float rate) {
    using namespace xtf;
    const float s0 = s.highpass[0];
    s.gate = add(mul(sub(target, s.gate), rate), s.gate);
    s.highpass[1] = add(mul(s0, kFeed), s.highpass[1]);
    const float y = sub(mul(x, s.gate), add(mul(s0, kBack), s.highpass[1]));
    s.highpass[0] = add(mul(y, kFeed), s0);
    return y;
}

void Vc2Unvoiced::consonants(float* frames, int channels, float gain, float flagScale, float levelMod) {
    using namespace xtf;
    const int at = s.micIndex - kMicLate < 0 ? s.micIndex - kMicLate + kMicRing : s.micIndex - kMicLate;
    s.micIndex = s.micIndex + kBlock >= kMicRing ? 0 : s.micIndex + kBlock;
    const float target = mul(lateFlag(), flagScale);
    float wanted = add(s.soundLevel, levelMod);
    if (0.0f > wanted) wanted = 0.0f;
    else if (wanted > 1.0f) wanted = 1.0f;
    level = add(mul(sub(wanted, level), kGlide), level);
    const float rate = target > s.gate ? kRise : kFall;
    for (int i = 0; i < kBlock; ++i) {
        float y = filtered(mic[(size_t)(at + i)], target, rate);
        if (s.spread != 0) {
            y = allpass(allpassB, allpassAt, kDelayB, allpass(allpassA, allpassAt, kDelayA, y));
            allpassAt = allpassAt - 1 < 0 ? allpassAt - 1 + kAllpass : allpassAt - 1;
        }
        const float added = mul(y, gain);
        for (int c = 0; c < channels; ++c) frames[2 * i + c] = add(mul(frames[2 * i + c], level), added);
    }
}
