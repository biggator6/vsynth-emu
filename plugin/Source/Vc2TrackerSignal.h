#pragma once
// xt_vc2tracker's input (v224): a block at a time, stretches long enough for the pitch tracker to lock.  Held tones of
// 55 to 1760 Hz with one to five harmonics, each 0.6 of the last, from 1e-4 to full scale, starting at their level (an
// onset), and half the time stepping to a new pitch at the same level; a breathy tone, with noise; noise; silence; and
// Vc2Signal's short stretches of anything.  A fading case plays one tone, then silence to its end.  XT_VC2TRACKER_MAIN
// only.
#include "Vc2CheckRig.h"
#include <cstdint>
#include <random>

// A stretch's kind, which says what it mixes and how long it lasts
enum class Vc2TrackerStretch { tone, breathy, noise, silence, rough };

class Vc2TrackerSignal {
public:
    Vc2TrackerSignal(uint32_t seed, bool fades);
    void block(float* x);       // kVc2Block samples
    float tonePeriod() const;   // the period of the tone it plays, in samples; 0 when it plays none

private:
    static constexpr int kPartials = 5;
    void draw();

    std::mt19937 rng_;
    bool fades_;
    Vc2Signal rough_;
    Vc2TrackerStretch kind_ = Vc2TrackerStretch::silence;
    int left_ = 0, drawn_ = 0, partials_ = 1;
    float level_ = 0.0f, hz_ = 0.0f;
    double phase_[kPartials] = {};
};
