#pragma once
// The analog OSC's knobs and the converter's block (v336: moved as they stood from XtAnalog.h, which includes this, so
// its includers see them as before; the code in XtAnalog.cpp).
#include <cstdint>

// The waveform's code (+0x80) for the patch's 0…13; and whether the port
// plays it.
int xtAnalogCode(int wave);
bool xtAnalogPlays(int wave);

// The analog knobs as the patch stores them (IDs 0x2b1…0x2ba for OSC 1):
// Waveform 0…13 (SAW); Impact 0…40 (0.0…4.0, default 10); Pulse Width
// −63…+63, its Keyfollow ±20 (×10 = the displayed ±200), LFO and Env Depth
// ±63; Fat 0…127 and the same; Detune ±63 (ID 0x30d), used by SUPER-SAW,
// FEEDBACK-OSC and the sub oscillator; Sub Octave 0…3 (ID 0x30e: OFF, −2, −1
// and 0 octaves) and Sub Level 0…127 (ID 0x30f, default 127).
struct XtAnalogParams {
    int wave = 0, impact = 10;
    int pw = 0, pwKf = 0, pwLfo = 0, pwEnv = 0;
    int fat = 0, fatKf = 0, fatLfo = 0, fatEnv = 0;
    int detune = 0;
    int subOctave = 0, subLevel = 127;
    bool operator==(const XtAnalogParams&) const = default;
};

// What the Matrix and the D-Beam's controllers add to a block's curves: the sums for Time/PW and Formant/
// Fat (the LFO depths' sums go in with the knobs), and the part's bytes at +0x180 (0x894e9fe0).
struct XtAnalogMod {
    int pw = 0, fat = 0, b0 = 0, b1 = 0;
};
// The converter's analog block:
//   code    +0x80, as above
//   fat     +0x84: Fat/127 + (key − 60)·KF·10/1200, within 0…1 (0x894e1010)
//   pw      +0x98: x = PW + (key − 60)·KF·10·126/1200 + 63, PW within ±63,
//           wrapped to 0…126 by its integer part, /126 (0x894e0ea0)
//   …Env    the envelope's velocity factor × (d/63)·|d/63| (+0x88, +0x9c)
//   …Lfo    (d/63)·|d/63|, d within ±63 (+0x90, +0xa0)
//   impact  +0xac: Impact·4/40
//   slope   +0xb0: SAW, SUPER-SAW, FEEDBACK-OSC and X-MOD the Saw Slope/100,
//           SQUARE the Square Slope/100, with Alias Cancel on (0x894e14a0);
//           0 otherwise
//   gain    +0x5c: 2^(Wave Gain/6) × the waveform's level at the note's
//           pitch (0x894e1350): level curve(Level)/32768 × max(0, 1 + d·KF/
//           6e6), d = pitch10 − bias·1000, the upper KF above the bias point
//           and the lower below (with −d)
//           X-MOD (patch 13): pw +0x98 the modulation's depth, i/127 +
//           (key − 60)·KF/12000 within 0…1 with i = PW + 64 (below 2: 0), and
//           its depths; all 0 on OSC 2 (0x894e208a)
//           SUPER-SAW (patch 11) has its own curves: +0x98 the saws' spread
//           from the Pulse Width, +0x84 the side saws' level from the Detune
//           knob with Fat's KF, each with its depths; FEEDBACK-OSC (12) too:
//           +0x98 the second tap's place, +0x84 its level
//   tone    +0xbc: LA-SQUARE's Tone (ID 0x529), the others' LA Saw Tone (0x528)
//   toneKf  +0xc0: LA-SQUARE's Tone Keyfollow (0x52b), the others' (0x52a)
//   subOn   +0xb4: 1 with Sub Octave above 0 on patch waveforms 0…10, else 0.
//           Then fat holds the second wave's pitch in octaves: the Detune
//           curve (0x894e1090) with Fat's KF, + −2, −1 or 0 by Sub Octave;
//           fatEnv and fatLfo are its depths, each d through the same curve,
//           /1200 (the envelope's × its velocity factor).  The curve: |d| to
//           10: d; to 15: 2d − 10; to 31: 5d − 55; to 51: 25d − 675; above:
//           50d − 1950, in cents
//   subLevel +0xb8: level curve(Sub Level)/32768
// The envelopes: Fat's is the OSC's third (Env 3, OSC 2 Env 7; the formant
// slot +0x974), the Pulse Width's its second (Env 2/6, the time slot +0x978);
// both LFOs are the OSC's (LFO 1/2).
struct XtAnalogBlock {
    int code = 2;
    float fat = 0, fatEnv = 0, fatLfo = 0;
    float pw = 0.5f, pwEnv = 0, pwLfo = 0;
    float impact = 1.0f, slope = 0, gain = 1.0f;
    float tone = 0, toneKf = 0;
    float subOn = 0, subLevel = 1.0f;
};
// key: the note (the converter's +3); pitch10: its pitch in 1/1000
// semitone (+0x74, xtPitch10 + the fine); velFat, velPw: the envelopes'
// velocity factors (1 with no velocity curve).
// osc2: the input's +2 (only X-MOD's block depends on it without a matrix).
// one-off: moved as it stood from XtAnalog.h (v336); its eight arguments are XtSynth's and the checks' calls, which a
// move doesn't reshape
XtAnalogBlock xtAnalogBlock(const XtAnalogParams& p, int key, int pitch10, int waveGainDb, float velFat = 1.0f,
                            float velPw = 1.0f, bool osc2 = false, const XtAnalogMod& mod = {});
