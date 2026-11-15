// The patch TVA on a voice: its new block, its gains and its part of the tone update (moved from XtSynth.cpp as they
// stood, v200).
#include "XtSynth.h"
#include "XtSynthConstants.h"
#include "XtFloat.h"

#include <algorithm>

using namespace xtsynth;

void XtSynth::tvaNewBlock(Voice& v) const {
    const float l = v.tb.bus[XtBusDirL], r = v.tb.bus[XtBusDirR];
    v.tb = tvaBlock(v);
    v.tb.bus[XtBusDirL] = l;
    v.tb.bus[XtBusDirR] = r;
}

// A gain or target (0x894f5360, and 0x894f50e0 at note-on): (1 + LFO·depth)·level·env
// times (1 ∓ LFO·pan depth), each channel within ±1.  With both depths 0 the
// factors are exactly 1.
void XtSynth::tvaGains(const Voice& v, float levelEnv, float* lr) const {
    using namespace xtf;
    const float x = mul(add(mul(v.tlfo.out(), v.tb.lfoDepth), 1.0f), levelEnv), p = mul(v.tlfo.out(), v.tb.panLfoDepth);
    lr[0] = std::clamp(mul(x, sub(1.0f, p)), -1.0f, 1.0f);
    lr[1] = std::clamp(mul(x, add(p, 1.0f)), -1.0f, 1.0f);
}

// The tone update's TVA part (0x894f75f0 tail): stop a finished voice
// (0x894f54b0), else aim at the envelope's new output with a linear ramp of
// 10 ms, shorter when the stage itself is (0x894f5360).
void XtSynth::tvaUpdate(Voice& v) {
    using namespace xtf;
    // this test does not ask whether the tone is truncated, so a finished voice truncated just before a tick stops here
    if (v.env.done() && v.cur[0] <= kSilent && v.cur[1] <= kSilent) { v.active = false; return; }
    if (v.damping) return;                                    // done when the fade is (0x894f5510)
    tvaGains(v, mul(v.env.level(), v.tb.level), v.target);
    const float T = v.env.stageTime();
    float frames = 441.0f;
    if (!(0.0f > T)) {
        const float d = sub(T, mul(v.elapsed, kInvMs));       // the stage time less the time since the last update
        if (d > 0.0f) { if (10.0f > d) frames = mul(d, kFramesPerMs); }
        else frames = mul(kNoAttackMs, kFramesPerMs);
    }
    v.step[0] = div(sub(v.target[0], v.cur[0]), frames);
    v.step[1] = div(sub(v.target[1], v.cur[1]), frames);
    v.elapsed = 0;
}
