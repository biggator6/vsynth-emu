#pragma once
// The V-Synth XT's analog oscillator (OSC Type ANALOG).  An OSC's knobs go
// through the analog converter (0x894e1920, reached from 0x894e48a0 for OSC
// Type 0) into the block the voice reads; the voice's install (0x89505170)
// puts one of fourteen DSP routines at +0x968 by the waveform, the note-on
// (0x89504af0) sets its state, the 10-ms update (0x89505400) forms new
// targets, and the per-block renderer (0x894f2800) runs the routine on 32
// frames: fn(out, voice + 0xd4, the other oscillator's sync mask), which
// returns the mask of samples where its phase wrapped.
//
// The waveforms, as the patch numbers them (ID 0x2b1/0x2e5) and as the
// converter codes them (+0x80, the routine's switch):
//   0 SAW 2, 1 SQUARE 3, 2 TRIANGLE 4, 3 SINE 6, 4 RAMP 5, 5 JUNO 7,
//   6 HQ-SAW 0, 7 HQ-SQUARE 1, 8 NOISE 8, 9 LA-SAW 10, 10 LA-SQUARE 9,
//   11 SUPER-SAW 11, 12 FEEDBACK-OSC 12, 13 X-MOD 13.
// The port plays the note-on's default case: HQ-SAW (0x895063f0), HQ-SQUARE
// (0x895060e0), SAW (0x89508c20), SQUARE (0x89508aa0), TRIANGLE
// (0x895094c0), SINE (0x89509030), RAMP (0x89508800), JUNO (0x89508db0) and
// NOISE (0x89504a40); LA-SQUARE (0x89508540), LA-SAW (0x895081f0), SUPER-SAW
// (0x89507520), FEEDBACK-OSC (0x89507900) and X-MOD (0x89507b60), installed at
// +0x96c rather than +0x968, which reads OSC 2's output.
//
// With the sub oscillator on (Sub Octave above 0: voice +0xb4 = 1.0) a second
// set runs: install 0x895069d0, note-on 0x89506700, update 0x89506bf0, and by the
// waveform SAW 0x89508f30, SQUARE 0x89509360, TRIANGLE 0x89509680, RAMP
// 0x89508960, SINE 0x89509230, JUNO 0x89508060, LA-SQUARE 0x89508670 and LA-SAW
// 0x89508350: two waves in one routine, each with its own phase and shape.
// HQ-SAW, HQ-SQUARE and NOISE keep their routines and play no second wave;
// SUPER-SAW, FEEDBACK-OSC and X-MOD have no sub oscillator (their converter
// cases set +0xb4 = 0).
//
// The level and the Saw's edge also follow hidden system parameters, the
// XT's analog calibration (IDs 0x528…0x55b, through 0x894e5f80 to the OSC
// table's +0xc204…): per waveform a key-follow bias point, upper and lower
// level KFs and a level; the Alias Cancel switch and the Saw and Square
// Slopes; the LA waveforms' Tones and Tone Keyfollows.  The port uses their
// defaults from rom.2's descriptors.
// Single precision as the SH-4 computes it (XtFloat.h); tables are read
// from the user's rom.2 at run time.
#include <cstdint>

bool xtAnalogLoadTables(const char* romPath);
bool xtAnalogTablesLoaded();

// What the firmware keeps for all voices: SUPER-SAW's phase counter
// (0x8987c894, 0 at startup), FEEDBACK-OSC's eighteen delay lines of signed
// bytes (0x89fe4e74) and their in-use flags (0x89ff0c4c).
struct XtAnalogShared {
    static constexpr int kLines = 18, kLineLen = 2700;
    float superSaw = 0;
    uint8_t lineUsed[kLines] = {};
    int8_t line[kLines][kLineLen] = {};
};

// MOD off, or MOD Type MIX (0x894f1d50 → 0x89527d80, at the converter's gains
// of 1: 0x894e0920): a += b·scale.  The firmware sums in its own units;
// scale is the exact power of two from b's units to a's.
void xtAnalogMix(float* lr, const float* b, float scale = 1.0f);

// The MOD section: one object at tone +0x1708, after both OSCs in Structure 0.
// The patch's values (input +6…+0xa, IDs 0x294…0x298): Switch, Type 0 MIX, 1
// RING, 2 FM, 3 ENV RING, 4 OSC SYNC, Original Level, Attack and Release
// Times, 0…127.
struct XtModParams {
    bool on = false;
    int type = 0, level = 127, attack = 0, release = 0;
};
// The block (tone block +0x5dc), which the converter (0x894e0920) writes:
//   off, or MIX    type 0, a = b = 1 (c left as it was)
//   RING, FM       a = 1, b = level curve(Level)/32768, c = 1
//   ENV RING       a = Attack/127, b = Release/127, c = 1
//   OSC SYNC       a = 1 (b and c left as they were)
// What it leaves stays in the block, and an update copies it into a held note.
struct XtModBlock {
    uint32_t type = 0;
    float a = 0, b = 0, c = 0;
};
void xtModBlock(const XtModParams& p, XtModBlock& b);
// The Types the port plays.
bool xtModPlays(uint32_t type);

// FM's delay lines, for all tones: 496 floats per line, the 24 tones' L lines
// (0x89f91c5c) followed by their R lines (0x89f9d65c).  A tone reads outside its own
// lines when the modulator passes full scale, so they are one array here too; past
// its ends the XT reads other memory, and the port reads 0.
struct XtModShared {
    static constexpr int kTones = 24, kLine = 496;
    float line[2 * kTones * kLine] = {};
};

// The MOD object.  The note-on (0x894f1b00, then the chooser 0x894f1d90) takes
// the type and the routine for the note; a new block on a held note (0x894f1c60)
// changes neither, only the targets, by the note's type.  So a held note goes on
// with its routine under gains from another Type's block.  All of the fields
// stay from note to note.
//   a, b, c   +4, +8, +0xc the targets, +0x18, +0x1c, +0x20 the current values:
//             each routine moves them 0.009375 of the way per sample, from the
//             difference at the block's start
//   MIX       (0x89527d80) out = L1·a + L2·b.  At gains of 1 the plain sum.
//   RING      (0x89527e00) out = ((L2·a + b)·c)·L1, with a's target (+4)/2^31,
//             both channels; b is the Original Level
//   FM        (0x89527b80, with either voice's stereo flag 0x89527c50) L1 goes
//             into the tone's line at +0x30, kept from note to note (the note-on
//             clears the lines); d = (L2 + (2^31 − 128))·a·1.15484e−7, so 0…496
//             samples at full scale; out = (line[w − ⌊d⌋ − 1]·f + line[w − ⌊d⌋]·(1 −
//             f) + L1·b)·c, each index brought back by 496 once.  c moves by (target
//             − c)·(2^31 − 128) per sample, which only an unwritten block reaches.
//             The first reads L only and writes both channels.
// process: L1 and L2 in the XT's units, 32 frames of interleaved L/R; out may be
// l1.
class XtMod {
public:
    // stereo1, stereo2: the voices' +0x960 as their setups leave them; osc1Analog:
    // OSC 1's kind is 3; tone: the tone's number (+0x3c), its lines in the pool
    void noteOn(const XtModBlock& b, bool stereo1, bool stereo2, bool osc1Analog, XtModShared& shared, int tone);
    void setBlock(const XtModBlock& b);
    void process(float* out, const float* l1, const float* l2);
    // MIX at gains of 1, targets and current values: the plain sum
    bool plain() const;

    // the state, for the firmware check
    uint32_t type() const { return type_; }
    uint32_t stereoCode() const { return stereo_; }
    uint32_t routine() const;                           // the routine's address (+0x50)
    float target(int i) const { return t[i]; }
    float current(int i) const { return g[i]; }
    int writeIndex() const { return w_; }

private:
    uint32_t type_ = 0, stereo_ = 0;
    bool analog_ = false;
    float t[3] = {}, g[3] = {};                         // a, b, c: the targets and current values
    XtModShared* shared_ = nullptr;
    int lineL = 0, lineR = 0, w_ = 0;                   // the tone's lines in the pool, +0x30
    float lineAt(int base, int i) const;
    float fmTap(int base, float m, float x, float a, float b, float c) const;
    void mix(float* out, const float* l1, const float* l2);
    void ring(float* out, const float* l1, const float* l2);
    void fm(float* out, const float* l1, const float* l2);
    void fmStereo(float* out, const float* l1, const float* l2);
};

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
};

// The converter's analog block (the matrix and controller terms 0):
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
XtAnalogBlock xtAnalogBlock(const XtAnalogParams& p, int key, int pitch10, int waveGainDb, float velFat = 1.0f, float velPw = 1.0f, bool osc2 = false);

// The analog voice: the fields after +0x144 that the note-on, the update and
// the routine keep.
//   ratio     +0x144, the pitch ratio (xtPitchTarget with no detune): set to
//             the target at note-on and at every update, without a glide
//   gain      +0x154 the target (0x894f3dc0), +0x158 the gain the routine
//             moves 1/32 toward it every sample; at note-on 0x894f3e20
//   fat, pw   +0x1c0/+0x1c4 and +0x1c8/+0x1cc, targets and current values;
//             the routine moves the current values 1/32 toward the targets
//             once a block (NOISE does not).  The Pulse Width's target by
//             the waveform: SAW folded to ½…1 and widened by the Slope;
//             TRIANGLE and SINE halved; the others as is
//   phase     +0x1d4, 0…1, kept from the last note: at note-on scaled by
//             Impact below 1 and set to 0 outside 0…1
//   impact    +0x1d8, added to the wave and multiplied by +0x1d0 every
//             sample: powf(3.05185e-5, ratio/100.227), so it falls 90 dB in
//             one period
//   flags     +0x1dc, the routine's half-cycle bits; NOISE's first random
//             state (0 at note-on)
//   edge      +0x1e0, SQUARE's edge half-width: the Slope × the ratio × a
//             step of the pitch (0 below G4, 0.05 to A4, … 0.25 from F#5)
//             × 220/44100
//   noise     +0x244, NOISE's second random state (kept from note to note);
//             +0x248, the samples it holds each value: 1 + 30·|1 − 2·PW|
//   hq        +0x1e4, HQ-SAW's and HQ-SQUARE's integrator; +0x1e8,
//             HQ-SQUARE's direction (both cleared at their note-on and by
//             a sync bit).  Their phase is in their own units (±P·…), so
//             the next note-on mostly finds it outside 0…1 and starts at 0.
//   la        LA-SQUARE and LA-SAW: +0x1f4 the phase's step; +0x224/+0x228 the
//             steepness' target and current value; +0x218 LA-SAW's second
//             wave.  Their gain, Fat and PW targets are per sample: gain
//             +0x154 = −gain·2^26, +0x1c0 and +0x1c8 the target × 0.002.
//   ss        SUPER-SAW: +0x24c/+0x250 the spread, +0x254/+0x258 the side saws'
//             level, targets and current values; +0x25c…+0x270 the side saws'
//             phases, +0x1d4 the centre's.  FEEDBACK-OSC's second tap's place
//             and level are in the same fields, and X-MOD's depth in +0x24c/
//             +0x250.  X-MOD's Fat and PW have no 1/32 move and no 70%: their
//             current values move 0.2 of the way each block.
//   fb        FEEDBACK-OSC: +0x274 the DC block's state, +0x278 the line's write
//             place, +0x27c which line.
//   sub       With the sub oscillator on the two waves' shapes, the level and
//             the gain move every sample (x·0.998 + target, the gain x·0.96875 +
//             target, so the targets are ×0.002 and ×2^26).  +0x1c0/+0x1c4 hold
//             the second wave's pitch against the first, 2^(octaves); +0x1c8/
//             +0x1cc the first wave's Pulse Width, +0x1ec/+0x1f0 the second's;
//             +0x1f4 and +0x1f8 the phases' steps; +0x1fc the second phase;
//             +0x200/+0x204 the level.  SQUARE and JUNO: +0x234/+0x238 and
//             +0x23c/+0x240 the steepness of each wave; LA-SQUARE and LA-SAW:
//             +0x224/+0x228 and +0x22c/+0x230; LA-SAW also +0x218 and +0x21c,
//             each wave's second wave.  RAMP: +0x208 and +0x210 the width of
//             its last segment; +0x208 stays on the voice and other waveforms'
//             note-ons read it.
class XtAnalogOsc {
public:
    // fatEnv/pwEnv: the envelopes' outputs (+0x4c); lfo: the LFO's (+0x38);
    // gain, gainStart: 0x894f3dc0 and 0x894f3e20 (xtOscGainTarget)
    void noteOn(const XtAnalogBlock& b, float ratio, float gain, float gainStart, float fatEnv, float pwEnv, float lfo, XtAnalogShared& shared);
    void update(const XtAnalogBlock& b, float ratio, float gain, float fatEnv, float pwEnv, float lfo);
    // 32 frames into lr (interleaved L/R, the same on both), in the XT's
    // units (±2^31 at full gain).  sync: bit i resets the phase at sample i;
    // on the XT only OSC 1 gets one, OSC 2's mask, and only with MOD Type OSC
    // SYNC (the tone's render).  Returns this oscillator's mask: bit i set
    // where a cycle's two halves have both been seen (NOISE: 0).  mod: X-MOD's
    // input, the other OSC's buffer in the XT's units, which it clears; null
    // when its depth is 0, as on OSC 2, where the firmware reads and clears a
    // buffer about to be written.  The others ignore it.
    uint32_t process(float* lr, uint32_t sync = 0, float* mod = nullptr);
    // the voice's stop (0x895053c0): FEEDBACK-OSC frees its delay line; once is
    // enough, and it does nothing for the other waveforms
    void release();

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
    uint32_t flags() const { return fl; }
    float edge() const { return edge_; }
    uint32_t noiseState() const { return s2; }
    int noiseHold() const { return hold; }
    float hqLevel() const { return hy; }
    int hqFalling() const { return hs; }
    float laStep() const { return laInc; }
    float laSteepTarget() const { return qT; }
    float laSteep() const { return qC; }
    float laPhase2() const { return ph2; }
    float detuneTarget() const { return ssDetT; }    // the spread
    float detune() const { return ssDetC; }
    float mixTarget() const { return ssMixT; }
    float mix() const { return ssMixC; }
    float sidePhase(int i) const { return ss[i]; }
    float fbState() const { return fbZ; }
    int fbWrite() const { return fbW; }
    int fbSlotIndex() const { return fbSlot; }
    bool subActive() const { return subOn_; }
    float subPwTarget() const { return swT; }
    float subPw() const { return swC; }
    float subStep() const { return subInc; }
    float subPhase() const { return sph; }
    float subLevelTarget() const { return lvlT; }
    float subLevel() const { return lvlC; }
    float rampEnd() const { return rampK; }
    float rampEnd2() const { return rampK2; }
    float laPhase3() const { return ph3; }
    float laSteep2Target() const { return q2T; }
    float laSteep2() const { return q2C; }
    float steepTarget() const { return sqT; }
    float steep() const { return sqC; }
    float steep2Target() const { return sq2T; }
    float steep2() const { return sq2C; }
    // the check's breakpoint cases: the points the routine compares the
    // phase with, as it forms them from the current Fat and PW (up to 8)
    int breakpoints(float* p) const;
    void setPhase(float p) { ph = p; }
    void setLaPhase2(float z) { ph2 = z; }
    void setSidePhase(int i, float p) { ss[i] = p; }
    void setSubPhase(float p) { sph = p; }
    void setPw(float w) { pwC = w; }
    void setLaPhase3(float z) { ph3 = z; }
    void setRatio(float r) { ratio_ = r; }

private:
    int code = 2;
    float ratio_ = 1.0f, gainT = 0, gainC = 0;
    float fatT = 0.5f, fatC = 0.5f, pwT = 0.5f, pwC = 0.5f;
    float ph = 0, imp = 0, dec = 0, edge_ = 0, hy = 0;
    float laInc = 0, ph2 = 0, qT = 0, qC = 0;
    float ssDetT = 0, ssDetC = 0, ssMixT = 0, ssMixC = 0, ss[6] = {};
    float fbZ = 0;
    int fbW = 0, fbSlot = -1;
    XtAnalogShared* shared_ = nullptr;
    uint32_t fl = 0, s2 = 0;
    int hold = 1, hs = 0;
    bool subOn_ = false;                                // +0xb4 at note-on
    float swT = 0, swC = 0, subInc = 0, sph = 0, lvlT = 0, lvlC = 0;
    float rampK = 0, rampK2 = 0, ph3 = 0, q2T = 0, q2C = 0;
    float sqT = 0, sqC = 0, sq2T = 0, sq2C = 0;
    // HQ-SAW's and HQ-SQUARE's lengths and factors for a block (samples ×
    // 32768): the period, the impulse's width, HQ-SQUARE's halves, the
    // phase scale, the table step, the impulse gain and the integrator leak
    struct HqFrame { float p = 0, pc = 0, l1 = 0, l2 = 0, k1 = 0, k2 = 0, k3 = 0, leak = 0; };
    HqFrame hqFrame() const;
    void shape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo, bool noteOn);
    void laShape(const XtAnalogBlock& b, float gain, float fatEnv, float pwEnv, float lfo);
    void laNoteOn(const XtAnalogBlock& b, float gain, float gainStart, float fatEnv, float pwEnv, float lfo);
    void ssShape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo);
    void ssNoteOn(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo, XtAnalogShared& shared);
    uint32_t superSaw(float* lr, uint32_t sync);
    void fbShape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo);
    void fbNoteOn(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo, XtAnalogShared& shared);
    uint32_t feedbackOsc(float* lr, uint32_t sync);
    void xmShape(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo);
    void xmNoteOn(const XtAnalogBlock& b, float fatEnv, float pwEnv, float lfo);
    uint32_t xMod(float* lr, uint32_t sync, float* mod);
    uint32_t laSquare(float* lr, uint32_t sync);
    uint32_t laSaw(float* lr, uint32_t sync);
    uint32_t hqSaw(float* lr, uint32_t sync);
    uint32_t hqSquare(float* lr, uint32_t sync);
    uint32_t saw(float* lr, uint32_t sync);
    uint32_t square(float* lr, uint32_t sync);
    uint32_t triangle(float* lr, uint32_t sync);
    uint32_t sine(float* lr, uint32_t sync);
    uint32_t ramp(float* lr, uint32_t sync);
    uint32_t juno(float* lr, uint32_t sync);
    uint32_t noise(float* lr);
    void moveShape();
    void subShape(const XtAnalogBlock& b, float gain, float fatEnv, float pwEnv, float lfo);
    void subNoteOn(const XtAnalogBlock& b, float gain, float gainStart, float fatEnv, float pwEnv, float lfo);
    uint32_t subPeak(float* lr, uint32_t sync, bool halfSync, bool shaped);
    uint32_t subSquare(float* lr, uint32_t sync);
    uint32_t subRamp(float* lr, uint32_t sync);
    uint32_t subJuno(float* lr, uint32_t sync);
    uint32_t subLaSquare(float* lr, uint32_t sync);
    uint32_t subLaSaw(float* lr, uint32_t sync);
};
