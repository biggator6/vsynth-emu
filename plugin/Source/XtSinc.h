#pragma once
// A polyphase windowed sinc (Kaiser, β 8): 2 × kHalf taps, kPhases phases and one past for the interpolation between
// them, each phase's taps summed to 1.  XtSynth's render, the XT's 44.1 kHz to the host's rate, moved here as it stood
// from XtSynth's prepare and render (v207), and the outside input the other way, the host's rate to the chip's 44.1 kHz
// (XtHostInput).  Not the firmware's: the XT runs at 44.1 kHz and has no rate to convert.
#include <vector>

class XtSinc {
public:
    static constexpr int kHalf = 16, kPhases = 512;
    // The kernel for a cutoff `fc` in cycles per source frame.  Not on the audio thread.
    void make(double fc);
    // Interleaved L/R `src`: both sides at `f` (0…1) of the way from frame i0 to i0 + 1, from the frames
    // i0 − kHalf + 1 … i0 + kHalf, the phases either side of f interpolated
    void at(const float* src, long i0, double f, float& l, float& r) const {
        constexpr int taps = 2 * kHalf;
        const double ph = f * kPhases;
        const int p0 = ph < kPhases - 1 ? (int)ph : kPhases - 1;
        const float a = (float)(ph - p0);
        const float* k0 = kernel_.data() + (size_t)p0 * taps;
        const float* k1 = k0 + taps;
        float sl = 0, sr = 0;
        for (int k = 0; k < taps; ++k) {
            const size_t s = (size_t)(i0 - kHalf + 1 + k);
            const float w = k0[k] + (k1[k] - k0[k]) * a;
            sl += src[2 * s] * w;
            sr += src[2 * s + 1] * w;
        }
        l = sl;
        r = sr;
    }

private:
    std::vector<float> kernel_;
};
