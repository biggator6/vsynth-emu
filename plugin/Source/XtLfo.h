#pragma once
// The V-Synth XT's LFO (rom.2): converter 0x894e0290; object 0x894f0b40 on
// a delay/fade base (vtable 0x89833ef8); note-on 0x894f0cf0 with trigger
// 0x894f0bf0; tick 0x894f1050 with rate 0x894f1320; note-off 0x894f0810.
// It steps once per 10 ms tick, as the envelopes do (the tone update), and
// its output (+0x38) is waveform + offset times the delay/fade amplitude.
// A rate from 128 on is a note value (LFO Rate "Note"): the LFO then follows the tempo
// clock (0x894f0fd0, each clock), and each tick goes as fast as it takes to reach the
// clock's phase by the next.  The rate, sine, offset and clock tables are read from the
// user's rom.2.
#include <cstdint>

bool xtLfoLoadTables(const char* romPath);
bool xtLfoTablesLoaded();

// The converter's block (copied to +0x4c…+0x68).
struct XtLfoParams {
    int wave = 1;            // 0 tri, 1 sine, 2 saw, 3 square, 4 trapezoid, 5 S&H, 6 random, 7 chaos
    float rate = 1.0f;       // Hz
    float delayMs = 0.0f, fadeMs = 0.0f;
    int keySync = 1;         // (key sync is chosen by the caller: note-on restarts the phase)
    int offset = 2;          // index into −1, −0.5, 0, +0.5, +1
    int fadeMode = 0;        // 0 ON <<, 1 ON >>, 2 OFF <<, 3 OFF >>
    int sync = 0;            // +0x14: a note value, `rate` its clocks (0x897cfeb8)
};
// Patch values: waveform 0..7 in the XT's order (SIN, TRI, SAW, SQR, RND,
// TRP, S&H, CHAOS), rate 0..127, then 128..144 the note values, offset 0..4
// (−100…+100), delay and fade 0..127 ((k/127)²·10 s), fade mode 0..3.
XtLfoParams xtLfoConvert(int wave, int rate, int offset, int delay, int fadeMode, int fade, bool keySync);

// The free-running phase a key-unsynced LFO starts from (0x894f0950; a pool at
// 0x89fa90b0 + 0x3040, one for each tone id and LFO number).  It runs at every
// control tick, sounding or not (0x894f09b0, from the scheduler 0x894f4d10), at
// the rate of the last note that took it (0x894f0a30): 1 Hz from power-on.  On a
// note value it follows the clock as an LFO does (0x894f0af0), with the time between
// clocks in place of the phase's distance.  The clock reaches it while the last note
// that took it had a note value (0x894f16d0, 0x894f1750), whether that note sounds or not.
class XtLfoFree {
public:
    void setRate(const XtLfoParams& p, float tempo);
    void tick(int n = 1);
    void clock(float dt);
    float phase() const { return ph; }
private:
    friend class XtLfo;
    float rateHz = 1.0f, ph = 0.0f;                 // +0x08, +0x10
    int sync = 0, wrap = 0, count = 0;              // +0x18, +0x28, +0x2c
    float step = 0.0f, target = 72.0f;              // +0x1c, +0x20
    float dt = 0x1.555554p-6f, tempo = 120.0f;      // +0x24 (1/48, 0x3caaaaaa), +0x30
    bool clocked = false;
};

class XtLfo {
public:
    // Note-on (0x894f0cf0): phase 0, fade per mode, then one tick with n = 0.
    // `chaos` seeds the chaos generator (+0x6c, 0..1), `tempo` is the note's (fr4,
    // OSC 1's block +0x60).
    void noteOn(const XtLfoParams& p, float chaos = 0.5f, float tempo = 120.0f);
    // Key-unsynced with no other tone of the part sounding (0x894f0e60): the same
    // from the free-running phase, which takes this note's rate.
    void noteOnFree(const XtLfoParams& p, float chaos, XtLfoFree& free, float tempo = 120.0f);
    // Key-unsynced with another sounding (0x894f0db0): the same from that tone's
    // LFO, its phase, chaos state and held values.
    void noteOnFollow(const XtLfoParams& p, const XtLfo& other);
    void noteOff();
    // New settings while it runs (0x894f0f40): the phase and the fade state
    // carry on, the times and rates are set again.
    void update(const XtLfoParams& p);
    void tick(int n = 1);
    // A clock of the tempo (0x894f0fd0): dt the seconds since the last.
    void clock(float dt);
    float out() const { return o; }
    float phase() const { return ph; }
    float amplitude() const { return amp; }
    // What the object keeps of a note value (+0x84, +0x88, +0x8c, +0x90, +0x94).  Only a
    // note on the tempo sets them, so another note may read what a note before it left
    // (a tone is not cleared when it is constructed); xt_tone starts the firmware's from these.
    float clockStep() const { return s84; }
    float clockTarget() const { return t88; }
    float clockTime() const { return d8c; }
    int clockWrap() const { return w90; }
    int clockCount() const { return c94; }

private:
    XtLfoParams p;
    // delay/fade base (+0x00…+0x20)
    int state = 0, dir = 1;
    int16_t count = 0;
    float tIn = 0, tOut = 0, delay = 0, rIn = 0, rOut = 0, rest = 0, target = 131072.0f, amp = 131072.0f;
    bool active = false;     // +0x28
    float ph = 0;            // +0x40
    float prev = 0, hold = 0; // +0x44, +0x48 (random waveforms)
    float x = 0.5f;          // +0x6c chaos state
    float o = 0;             // +0x38
    float t74 = 0;           // +0x74 the tempo
    float s84 = 0, t88 = 0, d8c = 0;   // +0x84 phase per clock, +0x88 the clock's phase, +0x8c seconds per clock
    int w90 = 0, c94 = 0;    // +0x90 the clock has wrapped, +0x94 clocks into the cycle
    float rate() const;
    float chaos();
    void start(const XtLfoParams& prm);
    void fadeStart();
    void fadeTick(int n);
    void setDir(int down);
    void setTimes();
};
