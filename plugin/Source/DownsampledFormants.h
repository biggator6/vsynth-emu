#pragma once

#include <array>
#include <vector>

namespace VSE {

// SourceFilterModel's downsampled formant analysis (Session 15; a class of its own in v176).  At 48 kHz a direct
// order-8/16 LPC wastes pole pairs on the 12–24 kHz noise floor (measured: poles at ~620 Hz, ~4.5 kHz, 12–20 kHz) and
// misses F2/F3 entirely.  This decimates the frame ×4 (≈12 kHz rate), runs order-12 LPC there, forcing every pole into
// the speech band, then maps the poles back to the full rate (θ/4, r^¼) and applies the formant shift to the angles.
//
// Its state is the smoothed autocorrelation the LPC runs on, frame to frame: the instance's own, restarted by reset().
// Until v176 it was a function-local thread_local, shared by every model processed on one thread and kept through a
// reset, so one processor's sound depended on another's and on the note before.
class DownsampledFormants {
public:
    static constexpr int kOrd = 12;              // ~5 formant pairs within the speech band
    using Lags = std::array<double, kOrd + 1>;   // an autocorrelation to lag kOrd, or a predictor's coefficients

    struct Section {   // 1/(1 + b1·z⁻¹ + b0·z⁻²)
        float b1, b0;
    };

    // The frame's formant sections, their angles scaled by `shiftRatio`, into `out`.  False on a degenerate frame:
    // the caller falls back to the full-rate path.
    bool analyse(const std::vector<float>& frame, float shiftRatio, std::vector<Section>& out);

    // The smoothing starts again at the next frame
    void reset() { smoothed_ = {}; }

private:
    // The frame's autocorrelation r smoothed with the frames before: a one-pole IIR on it normalised, α = 0.7
    void smooth(Lags& r);

    Lags smoothed_ {};   // normalised; [0] is 0 until a frame is in
};

} // namespace VSE
