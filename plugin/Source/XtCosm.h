#pragma once
// The V-Synth XT's COSM sections, TVF type (COSM Type 10).  Each oscillator
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
};
XtTvfBlock xtTvfBlock(const XtTvfParams& p, int key, float velA, float velB);

// The COSM velocity factors (0x894de3b0, +0x34/+0x38): the velocity curve
// (0x894ebd60) at the envelope's Velocity Curve and Sens (patch values,
// −63…+63 and 0…7, through 0x894ebed0/0x894ebe20) / 32768: A from Env 9
// (COSM 1) or 11, B from Env 10 or 12.
float xtCosmVelocity(int velocity, int patchSens, int patchCurve);

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
    // The 10-ms update (0x894ee6a0, type 13): targets and step rates.
    void update(float qEnv, float qRate, float qLfo, float cutEnv, float cutRate, float cutLfo);
    // 32 frames, interleaved L/R, in place (0x89502890).
    void process(float* lr);

    // The coefficients, for the firmware checks.
    float c1() const { return cur1; }
    float c2() const { return cur2; }
    float target1() const { return tgt1; }
    float target2() const { return tgt2; }
    const float* state() const { return s; }
    void setState(const float st[4], float cur1, float cur2, float tgt1, float tgt2, float rate1, float rate2);

private:
    XtTvfBlock blk;
    float s[4] = {};                  // the stages' states (D+0x1c…0x28)
    float cur1 = 0, cur2 = 0;         // the coefficients (D+0xbc, +0xc0)
    float tgt1 = 0, tgt2 = 0;         // their targets (D+0xa4, +0xa8)
    float rate1 = 0, rate2 = 0;       // the step rates (D+0xc4, +0xc8)
};

// The coefficients from the modulated values (0x894eecb0): Q + its depths ×
// the envelope and LFO, within −1…−1/64; Cutoff + its depths, through the
// table at ·127 (linearly between entries), at most the cap at 127 −
// trunc(−q·127); 0 below 0.
float xtTvfQ(float q, float envDepth, float env, float lfoDepth, float lfo);
float xtTvfCutoff(float cutoff, float envDepth, float env, float lfoDepth, float lfo, float q);
