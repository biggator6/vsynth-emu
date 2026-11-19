#pragma once
// xt_fx's input noise, moved from XtFxCheck.h as it stood.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"

namespace xtfxcheck {
// Noise into every input of both chips, a sample at a time: the frame's eight, the USB pair and (v204) the analog pair the
// input's routing reads, each at silence, low, mid or enough to saturate, a level held 256 samples
struct InputNoise {
    static constexpr unsigned kInputs[] = { 0x20, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x2c, 0x2e, 0x18, 0x1a, 0x10, 0x12 };
    static constexpr double kLevels[] = { 0.0, 0.002, 0.25, 2.0 };
    static constexpr int kCount = (int)(sizeof kInputs / sizeof *kInputs);
    Rng r;
    double level[kCount] = {};
    explicit InputNoise(uint32_t seed) : r(seed) {}
    // The sample's value at each input, a chip word (v274: the unit's check takes the frame's eight as a frame)
    void next(long sample, int32_t* x) {
        if (sample % 256 == 0) {
            const int w = r.below(kCount);
            level[w] = kLevels[r.below(4)];
        }
        for (int i = 0; i < kCount; i++) {
            const double v = level[i] * ((int32_t)r.next() / 2147483648.0) * 8388608.0;
            x[i] = (int32_t)std::clamp(v, -8388608.0, 8388607.0);
        }
    }
    void into(XtFxChip& chip, Reference& ref, long sample) {
        int32_t x[kCount];
        next(sample, x);
        for (int i = 0; i < kCount; i++) {
            chip.setGram(kInputs[i], x[i]);
            ref.runtime().writeGram(x[i], (uint8_t)kInputs[i]);
        }
    }
};
} // namespace xtfxcheck
#endif
