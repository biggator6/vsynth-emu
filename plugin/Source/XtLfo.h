#pragma once
// The V-Synth XT's LFO (rom.2): converter 0x894e0290; object 0x894f0b40 on
// a delay/fade base (vtable 0x89833ef8); note-on 0x894f0cf0 with trigger
// 0x894f0bf0; tick 0x894f1050 with rate 0x894f1320; note-off 0x894f0810.
// It steps once per 10 ms tick, as the envelopes do (the tone update), and
// its output (+0x38) is waveform + offset times the delay/fade amplitude.
// The rate, sine and offset tables are read from the user's rom.2.
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
};
// Patch values: waveform 0..7 in the XT's order (SIN, TRI, SAW, SQR, RND,
// TRP, S&H, CHAOS), rate 0..127, offset 0..4 (−100…+100), delay and fade
// 0..127 ((k/127)²·10 s), fade mode 0..3.  Tempo-synced rates are not
// modelled.
XtLfoParams xtLfoConvert(int wave, int rate, int offset, int delay, int fadeMode, int fade, bool keySync);

class XtLfo {
public:
    // Note-on (0x894f0cf0): phase 0, fade per mode, then one tick with n = 0.
    // `chaos` seeds the chaos generator (+0x6c, 0..1).
    void noteOn(const XtLfoParams& p, float chaos = 0.5f);
    void noteOff();
    // New settings while it runs (0x894f0f40): the phase and the fade state
    // carry on, the times and rates are set again.
    void update(const XtLfoParams& p);
    // A key-unsynced LFO follows a free-running one (0x894f0e60 takes its phase).
    void setPhase(float phase) { ph = phase; }
    void tick(int n = 1);
    float out() const { return o; }
    float phase() const { return ph; }
    float amplitude() const { return amp; }

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
    float rate() const;
    float chaos();
    void fadeStart();
    void fadeTick(int n);
    void setDir(int down);
    void setTimes();
};
