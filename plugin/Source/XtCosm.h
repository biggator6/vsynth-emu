#pragma once
// The V-Synth XT's COSM sections: the TVF (COSM Type 10), the Dynamic TVF (11),
// the Dual Filter (9), the TB Filter (16) and the Comb Filter (8; below
// XtCosmTvf).  Each oscillator
// pair's tone has two COSM sections in series between the oscillator mixer
// and the TVA (the tone render 0x894f6d40).  The patch's parameters go
// through a converter per type (0x894de3b0 → 0x894dd870 for the TVF) into a
// block; the voice's COSM object takes the block at note-on (0x894edf70:
// start values 0x894ec340, targets 0x894ed8f0), forms new targets every
// 10-ms update (vtable +0x14, 0x894ee6a0, after the envelopes and LFOs
// tick) and filters in 32-sample blocks (0x89502890 and one of twelve
// kernels, 0x89502050…0x89502840).
//
// The TVF is a chain of state-variable stages in the SH-4's single
// precision (round toward zero, FMAC as a product then a sum, XtFloat.h):
// c1 (from Q, −1…−1/64) and c2 (from Cutoff, through a 129-entry table in
// rom.2, capped by a 128-entry table indexed by Q) move toward their
// targets in eight steps per block.  It reads the left channel and writes
// both.  Tables are read from the user's rom.2 at run time.
#include <cstdint>
#include <vector>

bool xtCosmLoadTables(const char* romPath);
bool xtCosmTablesLoaded();

// The TVF's parameters as the patch stores them (IDs 0x920…0x927; ranges and
// defaults from rom.2's second descriptor bank): Filter Type 0…11 (LP24,
// BP24, HP24, NOTCH24, PEAK24, LP12, BP12, HP12, NOTCH12, PEAK12, LP6, HP6;
// default 5, LP12), Q 0…127, Q LFO and Env Depth ±63, Cutoff 0…127 (default
// 127), Cutoff Keyfollow ±20 (×10 = the displayed ±200%), Cutoff LFO and
// Env Depth ±63.
struct XtTvfParams {
    int filter = 5, q = 0, qLfo = 0, qEnv = 0, cutoff = 127, keyfollow = 0, cutoffLfo = 0, cutoffEnv = 0;
    int qOwn = 0, cutoffOwn = 0;      // Dynamic TVF only: Own Env Depth ±63 (IDs 0x92b, 0x930)
};

// The converter's block (0x894dd870; the matrix and controller terms 0):
//   q        Q·0.984375/127 − 1, Q within 0…127
//   cutoff   (key − 60)·KF·10/100/127 + Cutoff/127, within 0…1
//   …Lfo     (d/63)·|d/63|, d within ±63
//   …Env     the velocity factor × (d/63)·|d/63|: Cutoff with A, Q with B
// The envelopes are Env 10 (Q) and 9 (Cutoff) for COSM 1, 12 and 11 for
// COSM 2; the LFO is LFO 3 (COSM 1) or 4 (COSM 2).
struct XtTvfBlock {
    int mode = 5;
    float q = -1.0f, qEnv = 0, qLfo = 0, cutoff = 1.0f, cutoffEnv = 0, cutoffLfo = 0;
    bool dynamic = false;
    float qOwn = 0, cutoffOwn = 0;    // Dynamic TVF: (d/63)·|d/63|
};
XtTvfBlock xtTvfBlock(const XtTvfParams& p, int key, float velA, float velB);
// The Dynamic TVF (COSM Type 11, block code 14; 0x894dda40): the TVF's block
// plus the Q's and Cutoff's Own Env depths (parameters 0x928…0x931: Filter
// Type, Q, Q LFO, Q Own Env, Q Env, Cutoff, Cutoff KF, Cutoff LFO, Cutoff
// Own Env, Cutoff Env).
XtTvfBlock xtDtvfBlock(const XtTvfParams& p, int key, float velA, float velB);

// The COSM velocity factors (0x894de3b0, +0x34/+0x38): the velocity curve
// (0x894ebd60) at the envelope's Velocity Curve and Sens (patch values,
// −63…+63 and 0…7, through 0x894ebed0/0x894ebe20) / 32768: A from Env 9
// (COSM 1) or 11, B from Env 10 or 12.
float xtCosmVelocity(int velocity, int patchSens, int patchCurve);

// The Dynamic TVF's targets are the raw sums (0x894eda10, 0x894eeda0); per
// 32-sample block (0x89502920) a follower of the input level (0x895017e0:
// the sum of |L| over the block, the eighth frame of each eight counted
// twice, × 2^-34, on the XT's int32 scale; smoothed 10% a block) moves Q by
// its Own depth and the Cutoff by its, each depth slewing 0.001 a block
// toward the block's value (Q's toward its negative, from its positive at
// note-on: the XT's own quirk); Q is then clamped and the Cutoff tabled as
// the TVF's, per block.
class XtCosmTvf {
public:
    // Note-on (0x894edf70): the states cleared; the start values from the
    // envelopes' start levels (+0x58) and the LFOs (0x894ec340); the
    // targets from the envelopes' outputs (+0x4c, 0x894ed8f0); the step rates
    // 4 × the envelopes' follower coefficients (+0x54).
    void noteOn(const XtTvfBlock& b, float qEnvStart, float qEnvOut, float qRate, float qLfo,
                float cutEnvStart, float cutEnvOut, float cutRate, float cutLfo);
    // A new block while the note sounds (0x894ed8f0): new targets.
    void setBlock(const XtTvfBlock& b, float qEnv, float qLfo, float cutEnv, float cutLfo);
    // The 10-ms update (0x894ee6a0: type 13 0x894eecb0, type 14 0x894eeda0):
    // targets and step rates.
    void update(float qEnv, float qRate, float qLfo, float cutEnv, float cutRate, float cutLfo);
    // 32 frames, interleaved L/R, in place (0x89502890; 0x89502920 for the
    // Dynamic TVF).  `scale`: the buffer's units against the XT's (int16 ×
    // 65536), for the Dynamic TVF's follower.
    void process(float* lr, float scale = 1.0f);

    // The coefficients, for the firmware checks.
    float c1() const { return cur1; }
    float c2() const { return cur2; }
    float target1() const { return tgt1; }
    float target2() const { return tgt2; }
    const float* state() const { return s; }
    void setState(const float st[4], float cur1, float cur2, float tgt1, float tgt2, float rate1, float rate2);
    // The Dynamic TVF's follower (D+0x98) and slewed Own depths (D+0x90 Q,
    // +0x94 Cutoff).  The level is not cleared at note-on: it carries on from
    // the voice's previous note, as on the XT.
    float level() const { return lev; }
    float qOwnSlewed() const { return qOwnS; }
    float cutoffOwnSlewed() const { return cutOwnS; }
    void setFollower(float qOwnSlewed, float cutoffOwnSlewed, float level) { qOwnS = qOwnSlewed; cutOwnS = cutoffOwnSlewed; lev = level; }

private:
    XtTvfBlock blk;
    float s[4] = {};                  // the stages' states (D+0x1c…0x28)
    float cur1 = 0, cur2 = 0;         // the coefficients (D+0xbc, +0xc0)
    float tgt1 = 0, tgt2 = 0;         // their targets (D+0xa4, +0xa8)
    float rate1 = 0, rate2 = 0;       // the step rates (D+0xc4, +0xc8)
    float qOwnS = 0, cutOwnS = 0, lev = 0;
    void targets(float qEnv, float qLfo, float cutEnv, float cutLfo);
};

// The Dual Filter (COSM Type 9, block code 12): two of the TVF's 24-dB
// stages, each with its own Frequency, Q and depths.  Parameters 0x915…0x91f:
// Filter Type 0…2 (LPF/HPF, LPF->HPF, BPF/BPF; default 0); per filter
// Frequency 0…127 (90, 80), Frequency Keyfollow ±20 (10, 10), Q 0…127 (108,
// 120), LFO Depth and Env Depth ±63.  Filter 1 takes Env A (Env 9, COSM 2
// Env 11) and its velocity factor, Filter 2 Env B (10, 12); both LFO 3 (4).
struct XtDualParams {
    int type = 0, freq1 = 90, kf1 = 10, q1 = 108, lfo1 = 0, env1 = 0;
    int freq2 = 80, kf2 = 10, q2 = 120, lfo2 = 0, env2 = 0;
};

// The converter's block (0x894dd690): per filter the TVF's sums (0x894dc660
// for the Frequency and its keyfollow), Env depth × A (Filter 1) or B.
struct XtDualBlock {
    int mode = 0;
    float cut1 = 0, env1 = 0, q1 = -1.0f, lfo1 = 0, cut2 = 0, env2 = 0, q2 = -1.0f, lfo2 = 0;
};
XtDualBlock xtDualBlock(const XtDualParams& p, int key, float velA, float velB);

// The object for block code 12.  Its coefficients are both cutoffs: each the
// TVF's table step (0x894ec140) at Frequency + its depths × the envelope and
// LFO, capped by the filter's own Q.  The arguments go by the object's slots:
// 1 Filter 1 (Env A), 2 Filter 2 (Env B).  The kernel reads both Qs from the
// block as they stand (0x894fd090), so a new block changes them at once.
class XtCosmDual {
public:
    // Note-on (0x894edf70): the states cleared, the coefficients from the
    // envelopes' start levels (0x894ec340), the targets from their outputs
    // (0x894ed5d0), the step rates 4 × the envelopes' follower coefficients.
    void noteOn(const XtDualBlock& b, float env1Start, float env1Out, float rate1, float lfo1,
                float env2Start, float env2Out, float rate2, float lfo2);
    void setBlock(const XtDualBlock& b, float env1, float lfo1, float env2, float lfo2);    // 0x894ed5d0
    void update(float env1, float rate1, float lfo1, float env2, float rate2, float lfo2);  // 0x894ee6a0
    // 32 frames, interleaved L/R, in place (0x894fd090 → 0x894fcbe0, …cd70,
    // …cef0): reads L, writes both.
    void process(float* lr);

    float c1() const { return cur1; }
    float c2() const { return cur2; }
    float target1() const { return tgt1; }
    float target2() const { return tgt2; }
    const float* state() const { return s; }

private:
    XtDualBlock blk;
    float s[8] = {};                  // the four stages' states (D+0x1c…0x38)
    float cur1 = 0, cur2 = 0, tgt1 = 0, tgt2 = 0, rate1 = 0, rate2 = 0;
    void targets(float env1, float lfo1, float env2, float lfo2);
};

// The TB Filter (COSM Type 16, block code 20): a three-pole lowpass with
// resonance feedback into a two-pole highpass and a gain, its coefficients
// computed in Hz.  Parameters 0x94a…0x954: LPF Cutoff 0…127 (90), LPF
// Cutoff Keyfollow ±20, LPF LFO and Env depths ±63, LPF Q 0…127 (100), LPF
// Q Follow ±63 (25), HPF Cutoff 0…127 (45), HPF Keyfollow ±20, HPF LFO and
// Env depths ±63, Gain 0…127 (127).  The LPF takes Env A (Env 9, COSM 2
// Env 11) and its velocity factor, the HPF Env B (10, 12); both LFO 3 (4).
struct XtTbParams {
    int lpfCutoff = 90, lpfKf = 0, lpfLfo = 0, lpfEnv = 0, q = 100, qFollow = 25;
    int hpfCutoff = 45, hpfKf = 0, hpfLfo = 0, hpfEnv = 0, gain = 127;
};

// The converter's block (0x894de110): cutoffs and Q /127; keyfollows
// (KF·10 + 200)·4/400 − 2 (octaves per octave); Q Follow (F + 63)·2/126 − 1;
// LFO depths (d/63)·|d/63|; Env depths A or B × (d/63)·|d/63|; the gain from
// rom.2's table (0x897cfcb8) /32768 × 2; the key.
struct XtTbBlock {
    float gain = 0, lcut = 0, lkf = 0, q = 0, qf = 0, hcut = 0, hkf = 0, lenv = 0, llfo = 0, henv = 0, hlfo = 0;
    int key = 60;
};
XtTbBlock xtTbBlock(const XtTbParams& p, int key, float velA, float velB);

// The object for block code 20.  Its targets (0x894fbdc0 every 10 ms, and
// 0x894fc110 with a new block) come from the envelopes' and LFOs' outputs:
// slot 1 the LPF's (Env A), slot 2 the HPF's (Env B).  The LPF cutoff is
// 278·2^(G·P(c) + G' + modulation + 0.647)·2^(KF·(key − 60)/12) Hz (P a cubic,
// rom.2's exp2 tables 0x897d0228…), within 1…20,000 Hz; Q is shaped by Q
// Follow and the cutoff, within 0…7.9; the HPF is 2^(13·(c + mod)) Hz,
// within 1…10,000.  The coefficients are bilinear one-poles; the targets are
// kept ×0.01, and each coefficient moves 1% toward its target every sample.
class XtCosmTb {
public:
    // Note-on (0x894edf70 → 0x894fc110, 0x894fbd20): the targets, the
    // coefficients at them, the states cleared.
    void noteOn(const XtTbBlock& b, float env1, float lfo1, float env2, float lfo2);
    void setBlock(const XtTbBlock& b, float env1, float lfo1, float env2, float lfo2);  // 0x894fc110
    void update(float env1, float lfo1, float env2, float lfo2);                       // 0x894fbdc0
    // 32 frames, interleaved L/R, in place (0x89501950): reads L, writes both.
    void process(float* lr);

    // D+0x1c…0x28 the LPF's c0, c1, c2, Q; D+0x5c…0x64 the HPF's c0, c1 and
    // the gain (7); their targets ×0.01 at D+0x3c…, D+0x78…; the states
    // D+0x2c…0x38 (LPF) and D+0x68…0x74 (HPF).
    const float* coef() const { return c; }
    const float* target() const { return t; }
    const float* lpState() const { return s; }
    const float* hpState() const { return h; }

private:
    XtTbBlock blk;
    float c[7] = {}, t[7] = {}, s[4] = {}, h[4] = {};
    void targets(float env1, float lfo1, float env2, float lfo2);
};

// The Comb Filter (COSM Type 8, block code 9): the input less a slow DC
// follower, plus Tone × itself delayed by the note's period × the Octave's
// factor (4, 2, 1, ½, ¼ for −2…+2).  Parameters 0x90e…0x914: Octave Select
// 0…4 (3, +1), Detune ±100 with its LFO and Env depths ±63, Tone 0…127 with
// its LFO and Env depths.  Detune takes Env B (Env 10, COSM 2 Env 12) and its
// velocity factor, Tone Env A (9, 11); both LFO 3 (4).
struct XtCombParams {
    int octave = 3, detune = 0, detuneLfo = 0, detuneEnv = 0, tone = 0, toneLfo = 0, toneEnv = 0;
};

// The converter's block (0x894dd4f0): Tone ·2/127 − 1; Detune as is (±100);
// LFO depths (d/63)·|d/63|; Env depths B (Detune) or A (Tone) × (d/63)·|d/63|.
struct XtCombBlock {
    int octave = 3;
    float tone = -1.0f, toneEnv = 0, toneLfo = 0, detune = 0, detuneEnv = 0, detuneLfo = 0;
};
XtCombBlock xtCombBlock(const XtCombParams& p, float velA, float velB);

// The wave's root frequency, the voice's +0x9a4 (the voice setup
// 0x894f21c0): 440·2^((Original Pitch − 69)/12) Hz through exp2, for a wave
// without fine tuning; 440 with Robot Voice.
float xtCosmRootHz(int originalPitch, bool robot);

// The object for block code 9.  The delay in samples is 44100 · factor ·
// (1 − Detune/2400 + Env·depth + LFO·depth) / (pitch × root), within 0…3267:
// pitch the voice's pitch ratio (+0x144, as the 10-ms update leaves it; the
// object's +0), root its root frequency (+4).  Slot 1 is Detune's (Env B),
// slot 2 Tone's (Env A).  A new delay is taken only when the last has
// finished crossfading (500 samples); the buffer (3267 samples) is cleared at
// note-on (0x894ecb00).  Firmware quirks kept: a new block stores
// 1 − Detune/2400 where the update reads Detune, so after the first update the
// Detune is about −0.7 cents whatever its setting; Tone steps toward its target
// once per block, not every 4 samples; until a crossfade ends both reads use
// the old delay's fraction, which is not kept from block to block.
class XtCosmComb {
public:
    XtCosmComb();
    // Note-on (0x894edf70): buffer cleared, start values (0x894ec740) from the
    // envelopes' start levels, rates 4 × their follower coefficients, the
    // block (0x894ed390) with their outputs, then the delay settled.
    void noteOn(const XtCombBlock& b, float pitch, float root, float env1Start, float env1Out, float rate1, float lfo1,
                float env2Start, float env2Out, float rate2, float lfo2);
    void setBlock(const XtCombBlock& b, float env1, float lfo1, float env2, float lfo2);                    // 0x894ed390
    void update(float pitch, float env1, float rate1, float lfo1, float env2, float rate2, float lfo2);     // 0x894eea90
    // 32 frames, interleaved L/R, in place (0x894fa8d0): reads L, writes both.
    void process(float* lr);

    float delay() const { return dly; }
    float tone() const { return tn; }
    float toneTarget() const { return tnTgt; }
    float newDelay() const { return nDly; }
    float fraction() const { return frac; }
    float newFraction() const { return nFrac; }
    float progress() const { return prog; }
    float dc() const { return dcy; }
    int writeIndex() const { return w; }
    const float* buffer() const { return buf.data() + 1; }

private:
    XtCombBlock blk;
    float det = 1.0f;                 // +0x2c: 1 − Detune/2400 after a block
    float pitch = 1.0f, root = 440.0f;
    float dly = 0, tn = 0, tnTgt = 0, rate1 = 0, rate2 = 0;
    float nDly = 0, frac = 0, nFrac = 0, prog = 1.0f;
    float dcy = 0, dcK = 0;
    int w = 0;
    // the buffer, after one word that stays 0: at the longest delay (3267) the
    // firmware's second tap reads the word before its buffer (index −1)
    std::vector<float> buf;
    float delayFrom(float d, float env1, float lfo1) const;
    void newTarget(float d);
};

// The coefficients from the modulated values (0x894eecb0): Q + its depths ×
// the envelope and LFO, within −1…−1/64; Cutoff + its depths, through the
// table at ·127 (linearly between entries), at most the cap at 127 −
// trunc(−q·127); 0 below 0.
float xtTvfQ(float q, float envDepth, float env, float lfoDepth, float lfo);
float xtTvfCutoff(float cutoff, float envDepth, float env, float lfoDepth, float lfo, float q);
// The table step alone (0x894ec140): y within 0…1 (0 below 0) to the coefficient at Q.
float xtTvfCoef(float y, float q);
