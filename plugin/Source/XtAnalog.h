#pragma once
// The V-Synth XT's analog oscillator (OSC Type ANALOG).  An OSC's knobs go
// through the analog converter (0x894e1920, reached from 0x894e48a0 for OSC
// Type 0) into the block the voice reads; the voice's note-on (0x89504af0)
// installs one of fourteen DSP routines by the waveform (0x89505170), the
// 10-ms update (0x89505400) forms new targets, and the per-block renderer
// (0x894f2800) runs the routine on 32 frames: fn(out, voice + 0xd4, the
// other oscillator's sync mask).
//
// The waveforms, as the patch numbers them (ID 0x2b1/0x2e5) and as the
// converter codes them (+0x80, the routine's switch):
//   0 SAW 2, 1 SQUARE 3, 2 TRIANGLE 4, 3 SINE 6, 4 RAMP 5, 5 JUNO 7,
//   6 HQ-SAW 0, 7 HQ-SQUARE 1, 8 NOISE 8, 9 LA-SAW 10, 10 LA-SQUARE 9,
//   11 SUPER-SAW 11, 12 FEEDBACK-OSC 12, 13 X-MOD 13.
// The port plays SAW (code 2, 0x89508c20), without the sub oscillator.
//
// The level and the Saw's edge also follow hidden system parameters, the
// XT's analog calibration (IDs 0x528…0x55b, through 0x894e5f80 to the OSC
// table's +0xc204…): per waveform a key-follow bias point, upper and lower
// level KFs and a level; the Alias Cancel switch and the Saw and Square
// Slopes.  The port uses their defaults from rom.2's descriptors.
// Single precision as the SH-4 computes it (XtFloat.h); tables are read
// from the user's rom.2 at run time.
#include <cstdint>

bool xtAnalogLoadTables(const char* romPath);
bool xtAnalogTablesLoaded();

// The waveform's code (+0x80) for the patch's 0…13; and whether the port
// plays it.
int xtAnalogCode(int wave);
bool xtAnalogPlays(int wave);

// The analog knobs as the patch stores them (IDs 0x2b1…0x2ba for OSC 1):
// Waveform 0…13 (SAW); Impact 0…40 (0.0…4.0, default 10); Pulse Width
// −63…+63, its Keyfollow ±20 (×10 = the displayed ±200), LFO and Env Depth
// ±63; Fat 0…127 and the same.
struct XtAnalogParams {
    int wave = 0, impact = 10;
    int pw = 0, pwKf = 0, pwLfo = 0, pwEnv = 0;
    int fat = 0, fatKf = 0, fatLfo = 0, fatEnv = 0;
};

// The converter's analog block (the matrix and controller terms 0; the sub
// oscillator off, Sub Octave 0):
//   code    +0x80, as above
//   fat     +0x84: Fat/127 + (key − 60)·KF·10/1200, within 0…1 (0x894e1010)
//   pw      +0x98: x = PW + (key − 60)·KF·10·126/1200 + 63, PW within ±63,
//           wrapped to 0…126 by its integer part, /126 (0x894e0ea0)
//   …Env    the envelope's velocity factor × (d/63)·|d/63| (+0x88, +0x9c)
//   …Lfo    (d/63)·|d/63|, d within ±63 (+0x90, +0xa0)
//   impact  +0xac: Impact·4/40
//   slope   +0xb0: SAW the Saw Slope/100, SQUARE the Square Slope/100, with
//           Alias Cancel on (0x894e14a0); 0 otherwise
//   gain    +0x5c: 2^(Wave Gain/6) × the waveform's level at the note's
//           pitch (0x894e1350): level curve(Level)/32768 × max(0, 1 + d·KF/
//           6e6), d = pitch10 − bias·1000, the upper KF above the bias point
//           and the lower below (with −d)
// The envelopes: Fat's is the OSC's third (Env 3, OSC 2 Env 7; the formant
// slot +0x974), the Pulse Width's its second (Env 2/6, the time slot +0x978);
// both LFOs are the OSC's (LFO 1/2).
struct XtAnalogBlock {
    int code = 2;
    float fat = 0, fatEnv = 0, fatLfo = 0;
    float pw = 0.5f, pwEnv = 0, pwLfo = 0;
    float impact = 1.0f, slope = 0, gain = 1.0f;
};
// key: the note (the converter's +3); pitch10: its pitch in 1/1000
// semitone (+0x74, xtPitch10 + the fine); velFat, velPw: the envelopes'
// velocity factors (1 with no velocity curve).
XtAnalogBlock xtAnalogBlock(const XtAnalogParams& p, int key, int pitch10, int waveGainDb, float velFat = 1.0f, float velPw = 1.0f);

// The analog voice: the fields after +0x144 that the note-on, the update and
// the routine keep.
//   ratio     +0x144, the pitch ratio (xtPitchTarget with no detune): set to
//             the target at note-on and at every update, without a glide
//   gain      +0x154 the target (0x894f3dc0), +0x158 the gain the routine
//             moves 1/32 toward it every sample; at note-on 0x894f3e20
//   fat, pw   +0x1c0/+0x1c4 and +0x1c8/+0x1cc, targets and current values;
//             the routine moves the current values 1/32 toward the targets
//             once a block
//   phase     +0x1d4, 0…1, kept from the last note: at note-on scaled by
//             Impact below 1 and set to 0 outside 0…1
//   impact    +0x1d8, added to the wave and multiplied by +0x1d0 every
//             sample: powf(3.05185e-5, ratio/100.227), so it falls 90 dB in
//             one period
//   flags     +0x1dc, the routine's half-cycle bits
class XtAnalogOsc {
public:
    // fatEnv/pwEnv: the envelopes' outputs (+0x4c); lfo: the LFO's (+0x38);
    // gain, gainStart: 0x894f3dc0 and 0x894f3e20 (xtOscGainTarget)
    void noteOn(const XtAnalogBlock& b, float ratio, float gain, float gainStart, float fatEnv, float pwEnv, float lfo);
    void update(const XtAnalogBlock& b, float ratio, float gain, float fatEnv, float pwEnv, float lfo);
    // 32 frames into lr (interleaved L/R, the same on both), in the XT's
    // units (±2^31 at full gain).  sync: the other oscillator's mask, bit i
    // resetting the phase at sample i.
    void process(float* lr, uint32_t sync = 0);

    // the state, for the firmware check
    float ratio() const { return ratio_; }
    float gainTarget() const { return gainT; }
    float gain() const { return gainC; }
    float fatTarget() const { return fatT; }
    float fat() const { return fatC; }
    float pwTarget() const { return pwT; }
    float pw() const { return pwC; }
    float phase() const { return ph; }
    float impact() const { return imp; }
    float decay() const { return dec; }
    int flags() const { return fl; }
    void setPhase(float p) { ph = p; }     // the check's breakpoint cases

private:
    int code = 2;
    float ratio_ = 1.0f, gainT = 0, gainC = 0;
    float fatT = 0.5f, fatC = 0.5f, pwT = 0.5f, pwC = 0.5f;
    float ph = 0, imp = 0, dec = 0;
    int fl = 0;
    void shape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo, bool noteOn);
};
