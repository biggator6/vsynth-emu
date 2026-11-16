// The VC-2's pitch tracker's frame: see Vc2Tracker.h (v223).  Each routine's arithmetic in its own order, in XtFloat;
// every address is rom.4's.
#include "Vc2Tracker.h"
#include "XtFloat.h"
#include <cmath>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;

constexpr int kModes = 4;
constexpr int32_t kBoth = 2;                  // [FW] the reset's follow: both directions' estimates move P
constexpr int32_t kNoisyFrom = 15;            // [FW] the noise count from which the noisy-hold flag is set
constexpr int32_t kNoiseGate = 25;            // [FW] above it, no crossing is analysed
// [FW] each mode's candidate limits among the table setter's words
constexpr Vc2TrackerLimits Vc2TrackerSettings::* kModeLimits[kModes] = {
    &Vc2TrackerSettings::mode0, &Vc2TrackerSettings::mode1, &Vc2TrackerSettings::mode2, &Vc2TrackerSettings::mode3,
};

// The tracker's state-variable stage, a sample: low first, then band on in − (band + low), which it returns (its
// high-pass)
float trackerStage(Vc2TrackerStage& st, float in, float f) {
    st.low = add(mul(st.band, f), st.low);
    const float left = sub(in, add(st.band, st.low));
    st.band = add(mul(f, left), st.band);
    return left;
}
} // namespace

Vc2Tracker::Vc2Tracker(Vc2TrackerCrossings& crossings) : crossings_(crossings) {
    const Vc2TrackerTables& t = vc2TrackerTables();
    s.rate[0] = t.rate[0];
    s.rate[1] = t.rate[1];
    s.settings = t.settings;
}

void Vc2Tracker::reset() {
    s.lastUp = 0;
    s.sinceCrossing = s.lastSince = s.sinceUp = s.sinceDown = 0;
    s.analysisCbc = 0.0f;
    s.frameCount = 0;
    s.follow = kBoth;
    s.handOff = 0;
    setMode(0);
    s.peak = s.recentPeak = s.framePeak = s.lastFramePeak = 0.0f;
    for (float& w : s.crossing) w = 0.0f;
    for (float& h : s.halves) h = 0.0f;
    s.failures = s.lock = 0;
    s.estimate = s.analysisBdc = 0.0f;
    s.smoothing = 1.0f;
    s.filtered[0] = 0.0f;
    for (int32_t& c : s.counts) c = 0;
    s.noise = 0;
    s.lastInput = 0.0f;
    s.noisyHoldOn = s.noisyHold = 1;
}

void Vc2Tracker::setMode(int mode) {
    if (mode < 0 || mode >= kModes) return;
    const Vc2TrackerTables& t = vc2TrackerTables();
    const Vc2TrackerMode& m = t.modes[mode];
    const Vc2TrackerLimits& limits = s.settings.*kModeLimits[mode];
    s.modeHighpass = s.highpassTarget = m.highpass;
    s.modeLowpass = s.lowpassTarget = m.lowpass;
    s.period = s.fallback = m.period;
    s.shortest = limits.shortest;
    s.longest = limits.longest;
    s.lowpassF = t.lowpassStart;
    s.highpassF = t.highpassStart;
    s.highpass = s.lowpass = Vc2TrackerStage {};
    s.analysisCf8 = 0.0f;
}

void Vc2Tracker::filter(const float* in, int n) {
    const Vc2TrackerTables& t = vc2TrackerTables();
    const float glide = mul(xtf::flt(n), t.glide);
    s.lowpassF = add(mul(sub(s.lowpassTarget, s.lowpassF), glide), s.lowpassF);
    s.highpassF = add(mul(sub(s.highpassTarget, s.highpassF), glide), s.highpassF);
    for (int i = 0; i < n; ++i) {
        const float high = trackerStage(s.highpass, in[i], s.highpassF);   // the first's high-pass feeds the second
        trackerStage(s.lowpass, high, s.lowpassF);
        s.filtered[1 + i] = s.lowpass.low;
    }
}

void Vc2Tracker::track(int n) {
    float previous = s.filtered[0];
    for (int i = 0; i < n; ++i) {
        ++s.sinceUp;
        ++s.sinceCrossing;
        ++s.sinceDown;
        const float now = s.filtered[1 + i];
        const float magnitude = std::fabs(now);
        if (magnitude > s.peak) {
            s.peak = magnitude;
            if (magnitude > s.framePeak) s.framePeak = magnitude;
        }
        if (!(s.longest > xtf::flt(s.sinceCrossing))) s.fallback = s.period;
        if (s.noise <= kNoiseGate) {
            if (0.0f > now) {
                if (!(0.0f > previous)) crossings_.down(s, previous, now);
            } else if (0.0f > previous) {
                crossings_.up(s, previous, now);
            }
        }
        previous = now;
    }
    s.filtered[0] = previous;
}

void Vc2Tracker::countNoise(const float* in, int n) {
    const Vc2TrackerTables& t = vc2TrackerTables();
    float last = s.lastInput;
    int32_t count = 0;
    for (int i = 0; i < n; ++i) {   // a crossing of the raw input whose sample passes the crossing level
        const float x = in[i];
        if (0.0f > mul(x, last) && std::fabs(x) > s.settings.crossingLevel) ++count;
        last = x;
    }
    s.lastInput = last;
    s.noise = xtf::ftrc(add(add(mul(xtf::flt(s.noise), t.half), xtf::flt(count)), t.half));
    if (s.noisyHoldOn == 0) return;
    const bool quiet = s.settings.levels[2] > s.recentPeak;
    const int32_t noisy = s.noise >= kNoisyFrom ? 1 : 0;
    if (s.period == s.fallback) {   // P held: the flag set, if clear, when unlocked or quiet
        if (s.noisyHold == 0 && (s.lock == 0 || quiet)) s.noisyHold = noisy;
    } else if (s.noisyHold != 0) {  // P moved: the flag kept only while quiet
        s.noisyHold = quiet ? noisy : 0;
    }
}

float Vc2Tracker::block(const float* in, int n) {
    filter(in, n);
    s.frameCount += n;
    if (s.frameCount > s.settings.frameLength) {
        s.frameCount = 0;
        s.lastFramePeak = s.framePeak;
        s.framePeak = 0.0f;
    }
    track(n);
    countNoise(in, n);
    return s.period;
}
