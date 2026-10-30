#pragma once
// The V-Synth XT's VariPhrase OSC converter (rom.2 0x894e2f20, reached from
// 0x894e48a0 for a VariPhrase OSC; Energy in its encode-type tail 0x894e4030):
// the OSC knobs to the block the voice reads (+0x10 time, +0x14 formant,
// +0x18 Energy, +0x24/+0x2c the time/formant envelope depths, +0x3c/+0x44
// their LFO depths).  The voice update (0x894f3c90 walk, 0x894f3d40
// formant) adds the envelopes and the LFO to these every 10 ms.
//
// Each knob arrives as knob + matrix modulation (0x894d96d0).  The
// realtime-controller terms the firmware also adds (0x894e7010/0x894e7320
// and the part bytes at +0x180) are 0 here: no controller is assigned.
// Tables are read from the user's rom.2 at run time.

bool xtOscLoadTables(const char* romPath);
bool xtOscTablesLoaded();

// 2^(kf·(key − 60)/1200) as the firmware's powf computes it, kf = −200…+200
// in steps of 10, key 0…127 (41 × 128, from xtFwOscTimeKeyScales).  Without
// it, std::pow (which differs from the firmware's powf in the last bit).
void xtOscSetTimeKeyScales(const float* scales);

// Energy (+0x18): 0 = OFF, 1…127 → the level curve at 0x897cfcb8 / 32768,
// (k/127)² truncated.
float xtOscEnergy(int k);

// Time (+0x10, 0x894e0da0): the walk speed, ±5.
//   t      Time knob + modulation (the knob is −63…+63)
//   offset Time Offset: 0 BWD, 1 ZERO, 2 FWD (base speed −1, 0, +1)
//   kf     Time KF, % per octave (−200…+200)
// |t| ≤ 60 gives t/20 + base; 60…63 run on to ±5.  Times 2^(kf·(key−60)/1200).
float xtOscTime(int t, int offset, int kf, int key);

// The time envelope and LFO depths (+0x24, +0x3c): d/20, then to ±5 at ±63.
// The envelope's depth is used as is; the LFO's is clamped to ±63 first.
float xtOscTimeEnvDepth(int d);
float xtOscTimeLfoDepth(int d);

// Formant (+0x14), in octaves, ±1: f/63 (f clamped to ±63 through its
// 12000-unit scale) plus kf·(key − 60)/1200 (Formant KF, % per octave).
float xtOscFormant(int f, int kf, int key);

// The formant envelope and LFO depths (+0x2c, +0x44), octaves: d/63 (the
// LFO's clamped to ±63 first).
float xtOscFormantEnvDepth(int d);
float xtOscFormantLfoDepth(int d);

// The firmware's exp2 (0x894ec090): integer, high-byte and low-byte tables
// of a 16-bit fraction, for |x| < 7.
float xtOscExp2(float x);

// The voice's targets (0x894f3ac0/0x894f3c90 walk, 0x894f3b30/0x894f3d40
// formant ratio) from the converter's values and the envelope and LFO
// outputs: walk = (time + env·envDepth + lfo·lfoDepth), clamped to ±5;
// formant = 2^(formant + env·envDepth + lfo·lfoDepth), clamped to [0.5, 2].
float xtOscWalk(float time, float env, float envDepth, float lfo, float lfoDepth);
float xtOscFormantRatio(float formant, float env, float envDepth, float lfo, float lfoDepth);
