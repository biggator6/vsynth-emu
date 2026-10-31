#pragma once
// The V-Synth XT's COSM sections: the TVF (COSM Type 10), the Dynamic TVF (11)
// and the Dual Filter (9, below XtCosmTvf).  Each oscillator
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

// The coefficients from the modulated values (0x894eecb0): Q + its depths ×
// the envelope and LFO, within −1…−1/64; Cutoff + its depths, through the
// table at ·127 (linearly between entries), at most the cap at 127 −
// trunc(−q·127); 0 below 0.
float xtTvfQ(float q, float envDepth, float env, float lfoDepth, float lfo);
float xtTvfCutoff(float cutoff, float envDepth, float env, float lfoDepth, float lfo, float q);
// The table step alone (0x894ec140): y within 0…1 (0 below 0) to the coefficient at Q.
float xtTvfCoef(float y, float q);
