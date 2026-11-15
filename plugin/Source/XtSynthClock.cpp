// XtSynth's tempo clock: the clock object's period and readings, the host's transport as an outside clock, and a clock's
// tick on the tones (moved from XtSynth.cpp as they stood, v200).
#include "XtSynth.h"
#include "XtSynthConstants.h"
#include "XtFloat.h"

#include <algorithm>
#include <cmath>

using namespace xtsynth;

// The clock object at 0x89979bfc, from the Patch Tempo (0x89471ef0, its tenths dropped): the ms of
// a quarter, which is 480 ticks with its +0x10 at 20 (0x894714b0).  The converters read the tempo
// back from it (0x894e2d20), which is why 233 BPM plays at 233.4.
int XtSynth::clockPeriod() const {
    const int t = std::clamp(ctl.patchTempo, 200, 2500);
    return 3000 * 20 / (t / 10);
}

// The readers branch on the object's +0x40: from an outside clock, the ms over its last six clocks,
// which are a quarter of a quarter.  The limits are as they are: a reading under 20.0 BPM is 2.0, over 250.0 is 25.0.
int XtSynth::tempo10Now() const {
    const int t = clockExt ? 150000 / clockSix : 30000 * 20 / clockPeriod();
    return 200 > t ? 20 : t > 2500 ? 250 : t;
}

float XtSynth::tempoNow() const { return xtf::div(xtf::flt(tempo10Now()), 10.0f); }

int XtSynth::clocksDue() const {
    if (clockExt) {
        const double d = std::floor(hostAt + hostStep()) - (double)hostDone;
        return d > 0.0 ? (int)d : 0;
    }
    return (int)((clockAcc + kClockPerBlock) / ((uint64_t)clockPeriod() * 44100));
}

int XtSynth::msDue() const { return (int)((clockMsAcc + 32000) / 44100); }

double XtSynth::hostStep() const { return hostBpm * (32.0 * 24.0) / (60.0 * 44100.0); }

// 0x89471e30, flag 0x40 of the clock's task.  Going to EXTERNAL the object starts over, reading 120 BPM until its first count of six is done.
void XtSynth::clockFollow() {
    const bool ext = ctl.clockSource != 0 && hostBpm > 0.0;
    if (ext == clockExt) return;
    clockExt = ext;
    if (!ext) return;
    clockSeen = -1;
    clockSince = 0;
    clockSix = 125;
    hostDone = (int64_t)std::ceil(hostAt) - 1;
}

// An F8 (0x894712f0).  The object's ticks go on to the next 20, which is what makes it a clock (0x894d4210);
// between F8s the ticks stop short of that (0x894710e0, +0x4a), so there is one clock to an F8.  Every sixth: the count.
void XtSynth::clockCount() {
    if (++clockSeen < 6) return;
    clockSeen = 0;
    clockSix = clockSince ? clockSince : 1;
    clockSince = 0;
}

// Each ms (0x894710e0's end).  The count starts at the first clock, and is given up after 65536 ms short of six.
void XtSynth::clockMs() {
    if (clockSeen < 0) return;
    if (clockSince++ >= 0xffff) {
        clockSeen = -1;
        clockSince = 0;
    }
}

void XtSynth::hostClock(double bpm, bool moving, double ppq) {
    if (bpm > 0.0) hostBpm = bpm;
    clockFollow();
    if (!moving || !(hostBpm > 0.0)) return;
    // Where the next block will sound: the frames between the read position and it, as this host block's first frame stands at ppq
    const double at = (ppq + ((double)srcFrames - readPos) / 44100.0 * hostBpm / 60.0) * 24.0;
    if (std::fabs(at - hostAt) > 1.0) hostDone = (int64_t)std::ceil(at) - 1;   // a locate or a loop's end: what it passed gives no clocks
    hostAt = at;
}

// 0x894f4da0: the time since the last clock from the blocks passed, the free-running LFOs on a
// note value (0x894f17c0), then each tone: its eight LFOs (0x894f78a0) and its voices (0x894f78f0).
void XtSynth::clockTick() {
    using namespace xtf;
    constexpr float kBlockSeconds = 0x1.7ca212p-11f;   // 0.000726 (0x3a3e5109)
    float dt = mul(flt((int32_t)(clockBlocks - clockLast)), kBlockSeconds);
    if (dt == 0.0f) dt = kBlockSeconds;
    clockLast = clockBlocks;
    for (int z = 0; z < kZones; ++z) {
        freeLfo[z].clock(dt);
        freeLfo2[z].clock(dt);
        for (XtLfoFree& f : freeClfo[z]) f.clock(dt);
        freeTlfo[z].clock(dt);
    }
    for (Voice& v : voice) {
        if (!v.active) continue;
        auto oscClock = [&](OscVoice& s) {
            s.lfo.clock(dt);
            if (!s.on || s.analog || s.extIn) return;
            if (s.linear) { if (s.lin.syncOn()) s.lin.addClock(1); }
            else if (s.lite) { if (s.xfg.syncOn()) s.xfg.addClock(1); }
            else if (s.wave->type == 1) { if (s.solo.syncOn()) s.solo.addClock(1); }
            else if (s.mb.syncOn()) s.mb.addClock(1);
        };
        oscClock(v);
        oscClock(v.osc2);
        for (Voice::CosmVoice& c : v.cs) c.lfo.clock(dt);
        v.tlfo.clock(dt);
    }
}
