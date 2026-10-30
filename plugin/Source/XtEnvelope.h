#pragma once
// The V-Synth XT's envelope generator (rom.2), as its ADSR envelopes use it:
//   0x894f0130 note-on, 0x894efdf0 tick, 0x894ef520 stage rate,
//   0x894ef7d0 output, 0x894f03e0 note-off; parameters as the converter
//   0x894df240 fills them from the A/D/S/R knobs.
// The generator steps once per tick of the control clock; a stage of T ms
// takes T/10 ticks, so the tick is 10 ms (see HARDWARE_FIRMWARE.md, "The
// envelope generator").  The patch TVA ramps its gain to the output over
// each tick (XtSynth); smooth() is the coefficient per-sample followers use.
// The knob tables are read from the user's rom.2.
#include <cstdint>

// rom.2 tables: knob → time (ms), knob → sustain level, attack curve.
bool xtEnvLoadTables(const char* romPath);
bool xtEnvTablesLoaded();

// The envelope's parameter block (converter output, copied to +0x8…+0x3c).
struct XtEnvParams {
    int curve = 2;                  // output curve: 0 linear (pitch, time, formant), 2 shaped (amp, filter)
    float scale = 1.0f;             // time scale (velocity); the release is unscaled
    float time[4] = {};             // stage times, ms: attack, (none), decay, release
    float level[5] = {};            // start, peak, peak, sustain, end
};
// An envelope's knobs as the patch stores them.
struct XtEnvKnobs {
    int attack = 0, decay = 0, sustain = 127, release = 16;   // 0..127
    int aSens = 0, dSens = 0, rSens = 0;                        // velocity → time, −63..+63
    int timeKF = 0;                                             // −200..+200 (decay and release)
};
// The converter (0x894df240) for an ADSR with no matrix modulation: the
// attack time from the note-on velocity and A-Sens; decay and release from
// D-/R-Sens and the Time KF scale for `key`; the release from the note-off
// velocity.  The sens knobs go in as the patch's −63…+63 and are stored as
// the envelope table stores them, trunc(knob·100/63) (0x894df8b0).  Levels
// 0, 1, 1, S, 0.
XtEnvParams xtEnvAdsr(const XtEnvKnobs& k, int key = 60, int velocity = 64, int releaseVelocity = 64, int curve = 2);
inline XtEnvParams xtEnvAdsr(int attack, int decay, int sustain, int release, int curve = 2) {
    XtEnvKnobs k; k.attack = attack; k.decay = decay; k.sustain = sustain; k.release = release;
    return xtEnvAdsr(k, 60, 64, 64, curve);
}
// The time converter 0x894df7b0: ms from the knob table, less
// ((velocity − 64)·sens·40) >> 8 when sens ≠ 0 (then at most 12000), then
// the 16.16 time scale (result at least 1).  0: no time.
int xtEnvTime(int knob, int velocity, int sens, uint32_t scale16);
// 0x894df760: the Time KF scale 2^(−kf·(key − 60)/1200), 16.16.  The
// firmware's own values once xtEnvSetKeyScales has them (its powf is not
// correctly rounded: 251 of the 5248 values differ by a few units from
// std::pow); kf in steps of 10.
uint32_t xtEnvKeyScale(int key, int kf);
// 41 × 128 values, kf = −200, −190, … 200 by key (xtFwEnvKeyScales).
void xtEnvSetKeyScales(const uint32_t* table);
// 0x894e0ce0: a pitch depth knob (−63..+63) in cents — a table up to 12,
// then 50, 100, 200 and 400 cents per step (±1200 at 36, ±6000 at 63).
float xtPitchDepthCents(float depth);

class XtEnvelope {
public:
    static constexpr double kTickMs = 10.0;
    void noteOn(const XtEnvParams& p);
    void noteOff();
    // New times, levels and scale while the envelope runs (0x894f0080).
    void update(const XtEnvParams& p);
    void tick(int n = 1);
    float level() const { return out; }
    float smooth() const { return coef; }   // the consumer's per-sample coefficient (+0x54)
    int stage() const { return st; }         // 0 attack, 1, 2 decay, 3 sustain, 4 release, 5 done
    bool done() const { return st == 5; }
    int phase16() const { return phase; }
    // For the TVA: the stage's time in ms (−1 in sustain, +0x6c) and the level
    // its gain starts from at note-on (+0x58).
    float stageTime() const { return stT; }
    float onsetLevel() const { return onset; }

private:
    XtEnvParams p;
    int st = -1;                    // +0x00
    int rate = 0;                   // +0x40
    int phase = 0;                  // +0x44, 16-bit
    float out = 1.0f;               // +0x4c
    float rel = 0.0f;               // +0x50 release start
    float coef = 1.0f / 32.0f;      // +0x54
    float onset = 1.0f;             // +0x58
    float stT = 0.0f;               // +0x6c
    int stageRate() const;
    void linear();
    void shape();
    void output();
    void setCoef();
};
