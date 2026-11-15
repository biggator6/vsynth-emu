// The pad and Time Trip on the sounding voices, and a block's bits to a PCM OSC's player (moved from XtSynth.cpp as they
// stood, v203).
#include "XtSynth.h"

#include <algorithm>
#include <cmath>

// The pad reaches a tone in two ways.  A step goes straight to its voices (0x894f4e10 →
// 0x894f7920 → 0x894f25a0), those with Time Trip on.  Touching and letting go are seen at the
// next tone update (input +0x63 and the object's +8, 0x894e477a), which for the sounding
// tones here is now.
void XtSynth::padSteps(int step) {
    for (Voice& v : voice) {
        if (!v.active) continue;
        auto give = [&](OscVoice& s) {
            if (!s.on || s.analog || s.extIn) return;
            if (s.linear) { if (s.lin.tripOn()) s.lin.setTrip(step); }
            else if (s.lite) { if (s.xfg.tripOn()) s.xfg.setTrip(step); }
            else if (s.wave->type == 1) { if (s.solo.tripOn()) s.solo.setTrip(step); }
            else if (s.mb.tripOn()) s.mb.setTrip(step);
        };
        give(v);
        give(v.osc2);
    }
}

// 0x894d8360, as every controller reaches it: X and then Y, each if the number is its assign, of which
// 0 and 32 are none (0x894d82e0).  The two can be the same controller, and it is then both.
void XtSynth::padController(int number, int value) {
    const auto assigned = [number](int a) { return a != 0 && a != 32 && a == number; };
    if (assigned(ctl.assign[10])) pad.setX(value);
    if (assigned(ctl.assign[11])) {
        int step = 0;
        if (pad.setY(value, step)) padSteps(step);
    }
}

// The plugin's knob: the pad's two controllers in one breath, on no controller's number
void XtSynth::timeTrip(int x, int y) {
    const bool was = pad.on();
    int step = 0;
    pad.setX(x);
    const bool moved = pad.setY(y, step);
    if (moved) padSteps(step);
    if (pad.on() != was)
        for (Voice& v : voice)
            if (v.active) newBlocks(v);
}

void XtSynth::timeTripKnob(int value) {
    if (value <= 0) { timeTrip(0, 0); return; }
    const double a = -2.0 * 3.14159265358979323846 * (double)(std::min(value, 127) - 1) / 126.0;
    timeTrip(64 + (int)std::lround(63.0 * std::cos(a)), 64 + (int)std::lround(63.0 * std::sin(a)));
}

// 0x894f3ec0, a block's bits to a PCM OSC's player.  Letting go of the pad with Beat Keep on
// fades the player out, and the voice update stands aside until it is silent (0x894f20d2).
void XtSynth::oscBeat(OscVoice& s, const OscControls& o) {
    if (!s.on || s.analog || s.extIn) return;
    s.variBit = o.vari;                                // the voice's +0xb4, after a knob's update has worked out the walk (0x894f2788)
    s.syncBit = o.tempoSync;
    const bool sync = o.tempoSync, trip = o.timeTrip && pad.on();
    // A LITE wave's (0x8950bcc0, kind 1's) then gives its player the first and last samples from the voice's own block, and the loop from the
    // wave's record with its modes by the Loop bit, both as they stand: which after a LEGATO copy are this note's, not the earlier one's
    const XtLoop rec = s.lite ? waveLoop(s, o) : XtLoop {};
    if (s.linear) {
        if (o.beatKeep && s.lin.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.lin.setGainXt(0.0f); }
        s.lin.setBeatFlags(sync, trip);
        if (s.lite) s.lin.liteUpdate(rec, s.liteStart, (int32_t)s.wave->frames, s.curWalk);
    } else if (s.lite) {
        if (o.beatKeep && s.xfg.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.xfg.setGainXt(0.0f); }
        s.xfg.setBeatFlags(sync, trip);
        s.xfg.blockUpdate(rec, s.liteStart, (int32_t)s.wave->frames);
    } else if (s.wave->type == 1) {
        if (o.beatKeep && s.solo.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.solo.setGain(0.0f); }
        s.solo.setBeatFlags(sync, trip);
        s.solo.setRobot(s.robotKnob && o.vari);        // the block's byte, all but the Vari bit, to the player's (0x894f3fa8): Robot follows
        s.solo.setLoopBit(o.loop && !eventPlay(s, o)); // and the Loop bit, which Playback Mode's tails clear
    } else {
        if (o.beatKeep && s.mb.tripOn() && !trip) { s.chase = true; s.curGain = 0.0f; s.mb.setGainXt(0.0f); }
        s.mb.setBeatFlags(sync, trip);
        s.mb.setLoopBit(o.loop && !eventPlay(s, o));   // the block's byte to the player's (0x894f3fb6): the Loop bit, which Playback Mode's tails clear
    }
}
