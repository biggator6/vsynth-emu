#pragma once
// A reverb's decay from its impulse response, the measure-reverb skill's way (research/VC2_VOCAL_DESIGNER.md, "The new
// reverbs on XtFxChip"): the response less its DC, Schroeder's backward integral cut where the decay meets the floor and
// the floor taken off (Lundeby's way), a least-squares line through it.  vc2decay.cpp.
#include <cmath>
#include <cstdint>
#include <vector>

namespace vc2 {

// A response less its mean over its last tenth, where the effect chip's tails rest on a DC
std::vector<double> lessDc(const std::vector<int32_t>& x);

struct Decay {
    double seconds = NAN;   // T30, or T20 where the decay hasn't room for T30; NaN where it hasn't room for either
    int span = 0;           // 30 or 20: which of them
    double rangeDb = 0;     // the loudest 10 ms against the floor
    double floorDb = 0;     // the floor, a sample's energy against `reference`'s
    bool settled = false;   // the last tenth's halves within 3 dB: false when the response was still falling there,
                            // so the floor, and the time, are not to be read
};

// Two outs' decay (each already less its DC): their energy a sample, the floor its mean over the last tenth; the
// integral cut at the first 10 ms window after the loudest that is within 10 dB of the floor, the floor taken off each
// sample in it; the line from −5 to −35 dB, or to −25 dB where it doesn't reach −35
Decay decayOf(const std::vector<double>& l, const std::vector<double>& r, double rate, double reference);

} // namespace vc2
