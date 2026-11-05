#pragma once
// What plays a SOLO wave with Vari Sw off (rom.2 0x89509870 mono, 0x89509f52 stereo; the note-on in 0x894f3110): plain resampling of the wave's samples
// by linear interpolation, at pitch × walk, so that its time follows its pitch.  A band player's wave ignores the switch (0x894f3660).
// Bit for bit: plain float arithmetic under FE_TOWARDZERO, as the other players, FMAC a product then a sum.
//
// Its state is the voice's player object (voice +0xd4, here "P"):
//   +0xe8 fraction, +0xec position in samples, +0xf0 the samples, +0x124 the fade at the wave's end, +0x128 the position an Alternate loop runs on;
//   forward +0xf4 last sample, +0xf8, +0xfc the loop, +0x100 its length, +0x104 the rate in it, +0x108 Loop Mode;
//   backward +0x10c first sample, +0x110, +0x114 the loop, +0x118, +0x11c, +0x120.
// Past either end it holds that sample, fading.  In an Alternate loop the way back reads the other way with its sign flipped, as the firmware does.
// HARDWARE_FIRMWARE.md, "Vari off".
#include "XtBeat.h"
#include "XtLoop.h"
#include <cstdint>
#include <vector>

class XtLinearPlayer {
public:
    // Set before noteOn.  The backward rate on the XT is also × a word in the wave's cached events (+0x2d0), which no code seen writes, so 1 here.
    void setLoop(const XtLoop& l) { loopCfg = l; }
    // Tempo Sync and Time Trip (XtBeat), as XtSoloVoice's: the player keeps them apart from what it reads, and under either bit walks at the beat's step alone.
    void setTempo(uint32_t tempo, uint32_t orig) { tempoCfg = tempo; origCfg = orig; }
    void setBeatFlags(bool sync, bool trip) {
        if (sync && !(flags0 & 2)) beat_.clk = 0;
        if (trip && !(flags0 & 4)) beat_.trip = 1;
        flags0 = (uint8_t)((sync ? 2 : 0) | (trip ? 4 : 0));
    }
    void addClock(int n) { beat_.clk = (int8_t)(beat_.clk + n); }
    void setTrip(int step) { beat_.trip = (int8_t)step; }
    bool syncOn() const { return flags0 & 2; }
    bool tripOn() const { return flags0 & 4; }
    const XtBeat& beat() const { return beat_; }

    // A wave's samples, ±1 as XtSoloPlan holds them: `right` empty for mono.  They must outlive the note.
    void noteOn(const std::vector<float>& left, const std::vector<float>& right, float pitch, float walk);
    void setRates(float pitch_, float walk_) { pitch = pitch_; walk = walk_; }
    // +0x80 the target, +0x84 the gain: each sample the gain moves 1/32 of the way, then scales the sample.  `now` sets both.
    void setGainXt(float target, bool now = false) { gainTarget = target; if (now) gain = target; }
    float gainNow() const { return gain; }
    // For the check: +0xec, +0x128, +0xe8, +0x124
    int32_t position() const { return pos; }
    int32_t turningPos() const { return turning; }
    float fraction() const { return frac; }
    float fade() const { return ramp; }
    // 0x89509840: to a position in 1/16 sample, as a released Time Trip goes back to the beat.
    void seek(int32_t pos16);
    // 32 frames, L/R pairs, in the XT's units (int16 × 65536 × the gain).
    void block(float* lr);

private:
    const std::vector<float>* x = nullptr;
    const std::vector<float>* y = nullptr;
    int32_t sample(const std::vector<float>& v, int32_t i) const;
    void hold(float* lr, int32_t at, float sign);
    float pitch = 1.0f, walk = 1.0f;
    float frac = 0.0f, ramp = 1.0f, gainTarget = 0.0f, gain = 0.0f;
    int32_t pos = 0, turning = 0;                       // +0xec, +0x128
    struct Way { int32_t end = 0, a = 0, b = 0, len = 0; float rate = 1.0f; int mode = 0; };
    Way fwd, bwd;
    XtLoop loopCfg;
    XtBeat beat_;
    uint32_t tempoCfg = 1200000, origCfg = 1200000;
    uint8_t flags0 = 0;
    int32_t lp[4] = { 0, 0x7fffffff, 0x7fffffff, 0 };
};
