#pragma once
// The V-Synth XT's envelope generator (rom.2), as its ADSR envelopes use it:
//   0x894f0130 note-on, 0x894efdf0 tick, 0x894ef520 stage rate,
//   0x894ef7d0 output, 0x894f03e0 note-off; parameters as the converter
//   0x894df240 fills them from the A/D/S/R knobs.
// The generator steps once per tick of the control clock; a stage of T ms
// takes T/10 ticks, so the tick is 10 ms (see HARDWARE_FIRMWARE.md, "The
// envelope generator").  Its consumer follows the output per sample with the
// coefficient smooth().  The knob tables are read from the user's rom.2.
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
// ADSR knobs 0..127 as the converter maps them with no modulation, velocity
// or key follow.
XtEnvParams xtEnvAdsr(int attack, int decay, int sustain, int release, int curve = 2);

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

private:
    XtEnvParams p;
    int st = -1;                    // +0x00
    int rate = 0;                   // +0x40
    int phase = 0;                  // +0x44, 16-bit
    float out = 1.0f;               // +0x4c
    float rel = 0.0f;               // +0x50 release start
    float coef = 1.0f / 32.0f;      // +0x54
    int stageRate() const;
    void linear();
    void shape();
    void output();
    void setCoef();
};
