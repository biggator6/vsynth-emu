#pragma once
// XtAnalogOsc's words by their places in rom.2's analog voice's object (the voice's +0xd4, XtAnalog.h's map less 0xd4),
// for a port that runs its renders on another object's words, the VC-2's analog voice (v337, Vc2OscAnalogRender.cpp),
// as XtLitePlayer::words is LITE's heads' (v326).  SUPER-SAW's, FEEDBACK-OSC's and X-MOD's are not here: the VC-2 has
// none of them.  Audio thread.
#include "XtAnalog.h"
#include "XtWord.h"
#include <array>
#include <cstddef>
#include <span>

struct XtAnalogWords {
    // the ratio; the gain's target and value; Fat's and the Pulse Width's targets and values; the decay, the phase,
    // Impact, the flags (NOISE's first state), SQUARE's edge; HQ's integrator and direction; the second wave's Pulse
    // Width; the phases' steps (LA's, the sub oscillator's two), the second phase; the level; RAMP's segments; LA's
    // second and third waves; the steepnesses, LA's then SQUARE's and JUNO's; NOISE's second state and its hold (a byte)
    static std::span<const XtWord> words() {
        using O = XtAnalogOsc;
        constexpr size_t F = sizeof(float), W = sizeof(uint32_t), B = 1;
        static constexpr std::array<XtWord, 35> kWords { {
            { 0x70, offsetof(O, ratio_), F },
            { 0x80, offsetof(O, gainT), F },
            { 0x84, offsetof(O, gainC), F },
            { 0xec, offsetof(O, fatT), F },
            { 0xf0, offsetof(O, fatC), F },
            { 0xf4, offsetof(O, pwT), F },
            { 0xf8, offsetof(O, pwC), F },
            { 0xfc, offsetof(O, dec), F },
            { 0x100, offsetof(O, ph), F },
            { 0x104, offsetof(O, imp), F },
            { 0x108, offsetof(O, fl), W },
            { 0x10c, offsetof(O, edge_), F },
            { 0x110, offsetof(O, hy), F },
            { 0x114, offsetof(O, hs), W },
            { 0x118, offsetof(O, swT), F },
            { 0x11c, offsetof(O, swC), F },
            { 0x120, offsetof(O, laInc), F },
            { 0x124, offsetof(O, subInc), F },
            { 0x128, offsetof(O, sph), F },
            { 0x12c, offsetof(O, lvlT), F },
            { 0x130, offsetof(O, lvlC), F },
            { 0x134, offsetof(O, rampK), F },
            { 0x13c, offsetof(O, rampK2), F },
            { 0x144, offsetof(O, ph2), F },
            { 0x148, offsetof(O, ph3), F },
            { 0x150, offsetof(O, qT), F },
            { 0x154, offsetof(O, qC), F },
            { 0x158, offsetof(O, q2T), F },
            { 0x15c, offsetof(O, q2C), F },
            { 0x160, offsetof(O, sqT), F },
            { 0x164, offsetof(O, sqC), F },
            { 0x168, offsetof(O, sq2T), F },
            { 0x16c, offsetof(O, sq2C), F },
            { 0x170, offsetof(O, s2), W },
            { 0x174, offsetof(O, hold), B },
        } };
        return kWords;
    }
    // The routine an install picks, by the waveform's code and the sub oscillator's switch (XtAnalogOsc::process's
    // switch), HQ's tables, and the hold as the routine's MOV.B reads its byte, sign-extended
    static void take(XtAnalogOsc& o, int code, bool sub, const XtAnalogHq& hq) {
        o.code = code;
        o.subOn_ = sub;
        o.hq_ = hq;
        o.hold = (int8_t)o.hold;
    }
};
