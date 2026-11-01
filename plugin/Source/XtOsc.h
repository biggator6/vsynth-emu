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

#include <cstdint>

bool xtOscLoadTables(const char* romPath);
bool xtOscTablesLoaded();

// 2^(kf·(key − 60)/1200) as the firmware's powf computes it, kf = −200…+200
// in steps of 10, key 0…127 (41 × 128, from xtFwOscTimeKeyScales).  Without
// it, std::pow (which differs from the firmware's powf in the last bit).
void xtOscSetTimeKeyScales(const float* scales);
// One of them (kf −200…+200, key 0…127): the COSM Side Band Filters' Width
// keyfollow (0x894dc5f0) is the same power.
float xtOscTimeKeyScale(int kf, int key);

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

// ── Pitch ───────────────────────────────────────────────────────────────────
// The note's pitch in 1/1000 semitone (0x894e1250): (60 + Coarse)·1000 +
// Pitch KF·(key − 60) + Fine, with KF 0 and 1000 formed without the
// product.  Coarse in semitones; Pitch KF ×10 (the displayed −200…+200%:
// 1000 = 100%, the keyboard in semitones); Fine in 1/10 cents.
int xtPitch10(int key, int coarse, int kf, int fine);
// The voice's pitch in cents (+8, the encode-type tail 0x894e4030): that
// pitch against the wave's Original Key (SOLO Robot Voice: against A4,
// 69000), /10, within ±6000.
float xtPitchCents(int key, int coarse, int kf, int fine, int origKey, bool robot);
// The Pitch knob (+0xc): 2^(c/1200), c the depth curve's cents (0x894e0ce0)
// at the knob (−63…+63, + modulation, clamped).  The firmware's powf once
// xtOscSetPitchRatios has its 127 values (xtFwOscPitchRatios), else std::pow.
float xtPitchRatio(int pitch);
void xtOscSetPitchRatios(const float* ratios127);
// The player's pitch ratio (0x894f3a00 at note-on, 0x894f3ba0 in the
// update): the cents plus the pitch envelope's and the LFO's (their depths
// in cents, 0x894e0ce0), the portamento ramp (+0x880) and the wave's offset
// (+0x994), within ±6000; then exp2 (1/1200 as 0x3a5a740d), × the Pitch
// ratio × the wave's rate ratio (+0x990), within ±32; × exp2(−detune/1200)
// (+0x998).  The rate ratio is 44000 over the wave header's 44000, i.e. 1;
// offset and detune come from the wave's header and loop record (0 in every
// wave seen; 0 with Robot Voice); the ramp is 0 without portamento.
float xtPitchTarget(float cents, float env, float envDepth, float lfo, float lfoDepth, float ratio,
                    float ramp = 0.0f, float offset = 0.0f, float rate = 1.0f, float detune = 0.0f);

// The voice update's step toward a target (0x8950c1b0): 70% of the way,
// cur + (target − cur)·0.7 by FMAC.
float xtOscSmooth(float cur, float target);

// ── The note's tuning ───────────────────────────────────────────────────────
// The Coarse (+0x68) and Fine (+0x70) above, as the per-note OSC setup forms
// them (0x894e4940 at note-on, 0x894e4c30 when a voice is set up again;
// 0x894e4e90 forms them again on an update, keeping the random offset).
// The patch's terms, as the OSC table updater (0x894e6330) stores them per
// part: Coarse Tune (ID 0x110, semitones), Fine Tune (0x111, cents, ×10), and
// Scale Tune (switch 0x134; C…B at 0x136…0x14c, cents, ×10; 0 when off).
struct XtTuning {
    int coarse = 0, fine = 0;
    bool scaleOn = false;
    int scale[12] = {};
};
// The coarse: Patch Coarse + OSC Coarse (semitones), plus the system's Key
// Shift and the matrix's pitch terms (both 0 here).
int xtNoteCoarse(const XtTuning& t, int oscCoarse);
// The fine, 1/10 cent: the Scale Tune at key % 12 + Patch Fine + OSC Fine
// (cents) + the part's pitch bend + the random offset (both 1/10 cent), plus
// the system's Master Tune and the matrix's fine terms (both 0 here).
int xtNoteFine(const XtTuning& t, int key, int oscFine, int bend, int random);
// The part's pitch bend (0x894e9fe0, its +0x174), 1/10 cent: b = the bender
// (0…16383) − 8192; b·up·1000 >> 13 up, −(−b·down·1000 >> 13) down, with
// Bend Range Up/Down 0…48 (IDs 0x11e/0x11f, stored ×1000).
int xtPitchBend(int bender, int upSemis, int downSemis);

// Random Pitch Depth (OSC, ID 0x2ad, 0…30) in 1/10 cent, as the OSC updater
// (0x894e55d0) stores it: 0…9 cents in 1s, 10…100 in 10s, 200…1200 in 100s.
int xtRandomPitchDepth(int index);
// The XT's random pitch: one 16-bit generator for every voice, seeded 0x7ab7
// at power-on (0x894e5010).  Each OSC set up with a depth steps it, x =
// x·0x497d + 1, and takes ((x·depth) & 0x7fffffff) >> 15 as an int16: the
// offset (+0x6c), 1/10 cent, within ±depth.  With no depth it stays put.
class XtPitchRandom {
public:
    int next(int depth10);
    int state() const { return x; }
private:
    int16_t x = 0x7ab7;
};

// ── Portamento ──────────────────────────────────────────────────────────────
// The tone's portamento settings (0x894e66e0, the block every voice ramp
// copies at note-on): the switch; type 0 RATE, 1 TIME; the time in ms from
// Time (0..127, + modulation): k²·10000/127² (1 ms at 1), less t·v²·sens/
// 258064 (the product in 32 bits, as the firmware wraps it), at least 0;
// v = the velocity, or 127 − it for a negative Time Velo Sens (−63..+63).
// The time and formant ramps stay off (the glide is the pitch's only).
struct XtPortaSettings {
    bool on = false;
    int type = 0;
    float timeMs = 0;
};
XtPortaSettings xtPortaSettings(bool on, int type, int time, int veloSens, int velocity);

// The voice's pitch ramp (+0x880, the additive ramp, 0x894f4860 type 0):
// started at note-on from the previous key's cents to the note's (0x894f4890
// → 0x894f4680), after the note-on targets; ticked once per 10 ms update
// before the voice update (0x894f4b00; the first tick only arms it); its
// value is added to the pitch target's cents (0x894f4c00): −(to − from) at
// the start, then (phase/65536 − 1)·(to − from) as the phase grows by
// 65536·10/T per tick (T the time, or for RATE the time per 1200 cents of
// the span; at most 65536), 0 when done and when off.
// A legato note (the voice set up again, 0x894db8a0) first gives the ramp
// the note's settings (0x894f48b0: the switch and time, not the type; the
// increment again from the span; running or not, as it was), then, when the
// key-follow key (xtKeyFollowKey) has changed, starts it again from the
// previous key's cents to the note's with those settings (0x894f2600 →
// 0x894f4920, the same start as at note-on).
class XtPitchRamp {
public:
    void start(float from, float to, const XtPortaSettings& s);
    void update(const XtPortaSettings& s);
    void restart(float from, float to);
    void tick(int n = 1);
    float value() const { return active ? cur : 0.0f; }
    bool running() const { return active && state != 3; }
private:
    bool active = false, on = false;
    int state = 0, type = 0;
    float span = 0, phase = 0, inc = 0, cur = 0, timeMs = 0;
    void step();                         // the increment from the span and the time
};

// The OSC's key-follow key (+4, 0x894e12c0), which a legato note compares to
// restart the glide: KF 0: 60 + coarse; KF 1000: key + coarse; otherwise
// ((60 + coarse)·1000 + KF·(key − 60) + 999)/1000, toward zero; within
// 0…127.  KF as xtPitch10 takes it (×10).
int xtKeyFollowKey(int key, int coarse, int kf);

// ── Level (the OSC TVA) and velocity ────────────────────────────────────────
// Velocity Sens and Curve as the patch → table updaters store them:
// 0x894ebe20, ±63 → ±200 (s·100/32 up to ±32, then 100 + (|s| − 32)·100/31);
// 0x894ebed0, curve 1…7 → 0…6, 0 → 7 (no velocity effect).
int xtVelSens(int patchSens);
int xtVelCurve(int patchCurve);
// 0x894ebd60: the velocity factor 0…32768 for an internal sens and curve.
int xtVelCurveValue(int velocity, int sens, int curve);
// (c/32768)², the patch TVA's (0x894ea990) and the OSC level's velocity
// gain, from the patch's Velocity Sens (−63…+63) and Curve (0…7).
float xtVelGain(int velocity, int patchSens, int patchCurve);

// The firmware's powf(2, dB/6) for Wave Gain −12…+12 dB (xtFwOscWaveGains).
void xtOscSetWaveGains(const float* gains25);

// The OSC level (+0x54): the level curve at Level + Level KF·(key − 60)
// (0x894e0c90: level + trunc(127·kf·(key − 60)/6000), within 0…127), times
// the velocity gain.  Level 0…127 (+ modulation), Level KF −200…+200.
float xtOscLevel(int level, int kf, int key, float velGain);
// Level LFO depth (+0x58): (d/63)·|d/63|, d clamped to ±63.
float xtOscLevelLfoDepth(int d);
// The OSC gain (+0x5c): 2^(Wave Gain/6), times the level curve at two
// further levels (+0x84 / +0x86, set with the wave) taken as full here.
float xtOscGain(int waveGainDb);
// The player's gain target (0x894f3dc0): gain·clamp((1 + lfo·lfoDepth)·
// env·level, ±1), env the OSC amp envelope's output (at note-on,
// 0x894f3e20, its start level unless the attack time is 0).
float xtOscGainTarget(float gain, float level, float env, float lfo, float lfoDepth);
