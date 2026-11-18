#pragma once
// The Side Band Filters, XtCosm's COSM types 6 and 7 (block codes 7 and 8): their parameters, block and object, moved
// from XtCosm.h as they stood (v260); XtCosm.h includes this, and their code stays in XtCosm.cpp.
#include <algorithm>
#include <vector>

// The Side Band Filters (COSM Types 6 and 7, block codes 7 and 8): the input
// less the Comb's DC follower, through two delay lines tuned to the note (the
// Comb's delay, Octave Select 0…2 for −2…0, factors 4, 2, 1), each read with a
// crossfade between integer taps, a one-pole LPF and a first-order allpass for
// the fraction, fed back with the Side Band Width.  The 1st (0x8f3…0x900):
// HPF Cutoff 0…127 (48), LPF Cutoff 0…127 (64) with Keyfollow ±20 (10), Sub
// Delay Line Detune ±100, Octave Select (1), Detune ±100 with LFO, Own Env
// and Env depths ±63, Side Band Width 0…127 (16) with Keyfollow ±20 and the
// same depths.  The 2nd (0x901…0x90d): Q 0…127 (64) instead of HPF and Sub
// Delay, Width 32.  Detune takes Env B (Env 10, COSM 2 Env 12) and its
// velocity factor, Width Env A (9, 11); both LFO 3 (4).  The Own Envs follow
// the input level (the Dynamic TVF's follower).
struct XtSbfParams {
    bool second = false;
    int hpf = 48, q = 64, lpf = 64, lpfKf = 10, subDetune = 0, octave = 1;
    int detune = 0, detuneLfo = 0, detuneOwn = 0, detuneEnv = 0;
    int width = 16, widthKf = 0, widthLfo = 0, widthOwn = 0, widthEnv = 0;
};

// The converters' blocks (0x894dd0a0, 0x894dd2c0):
// - HPF −HPF/127; Q ·0.96875/127 − 1; Detune and Sub Detune as are (±100);
// - LPF (0x894dc540): Cutoff/2/127 + ½ ± ln 2 · x²/4, x = (key − 60)·KF/120
//   (the firmware's powf, xtCosmSetKeySquares), within ½…1;
// - Width (0x894dc5f0): Width/127 · 2^(KF·10·(key − 60)/1200), within 0…1;
// - LFO and Own depths (d/63)·|d/63|; Env depths B (Detune) or A (Width) ×
//   (d/63)·|d/63|.
struct XtSbfBlock {
    bool second = false;
    int octave = 1;
    float hpf = 0, q = -1.0f, lpf = 0.5f, subDetune = 0;
    float detune = 0, detuneEnv = 0, detuneLfo = 0, detuneOwn = 0;
    float width = 0, widthEnv = 0, widthLfo = 0, widthOwn = 0;
};
XtSbfBlock xtSbfBlock(const XtSbfParams& p, int key, float velA, float velB);

// The firmware's powf(x, 2) for the LPF keyfollow, KF −20…+20 by key 0..127
// (41 × 128, xtFwCosmKeySquares).  Without it, x·x in double.
void xtCosmSetKeySquares(const float* squares);

// The object for block codes 7 and 8 (0x89500640, 0x89500d40).  Per block:
// - the follower of the input level moves 0.001 toward the block's (as the
//   Dynamic TVF's, cleared at note-on); the Own depths slew 0.1 toward
//   their blocks', as do the LPF, HPF (½·HPF), Q and the Sub Detune factor
//   (1 − Sub/2400);
// - Width: its target + Own × level, within 0…1 (2nd: 0…0.95), stepped
//   toward 8 times a block by rate 2 (4 × Env A's coefficient); the gain
//   2(1 − |w|) + w² (0 at w = 0), approached 0.0125 a step;
// - once a crossfade has finished, the delay 44100 · factor · (Detune target
//   + Own × level) / (pitch × root), halved until under 3267 (1 below 1), and
//   a new one crossfades in over 132 samples.
// The 1st: two lines, the second at delay × the Sub Detune factor, each
//   tap → LPF → allpass a; v = (e − a)·w; line ← hs·v' + (1 + hs)·v + a
//   (v' the last v, hs the HPF); out = (line 1 + line 2) × gain.
// The 2nd: one delay, both buffers read: b ← A + w·B and a ← (Q·B + e − (A +
//   w·B))·w + B, A = LPF(allpass(tap b)), B = LPF(allpass(tap a)); out =
//   (A + w·B) × gain.
// Firmware quirks kept: the allpasses' fractions are the start delay's,
// never updated; the note-on gain uses the unclamped Width target.  Line 2's
// taps can reach up to 136 samples before its buffer: on the XT the end of
// the previous pool slot's buffer, another voice's; here zeros.
class XtCosmSbf {
public:
    XtCosmSbf();
    // Note-on (0x894edf70): states and buffers cleared, start values
    // (0x894ec5f0/0x894ec6b0) from the envelopes' start levels, rates 4 ×
    // their coefficients, the block (0x894ed1a0/0x894ed2a0) with their
    // outputs, then 0x894ee1a0/0x894ee260.
    void noteOn(const XtSbfBlock& b, float pitch, float root, float env1Start, float env1Out, float rate1, float lfo1,
                float env2Start, float env2Out, float rate2, float lfo2);
    // 0x894ed1a0/0x894ed2a0
    void setBlock(const XtSbfBlock& b, float env1, float lfo1, float env2, float lfo2);
    // 0x894ee9d0/0x894eea20
    void update(float pitch, float env1, float rate1, float lfo1, float env2, float rate2, float lfo2);
    // 32 frames, interleaved L/R, in place: reads L, writes both.  `scale`:
    // the buffer's units against the XT's (int16 × 65536), for the follower.  `stereo`:
    // 0x89500940, 0x89500fc0, R on buffers of its own; the follower reads both channels
    // (0x89501850, which runs three words past the block: `past`, absent zeros).
    void process(float* lr, float scale = 1.0f, bool stereo = false, const float* past = nullptr);

    // The state, for the firmware checks (D = object + 0x58).
    float delay() const { return dly; }              // D+0xbc
    float newDelay() const { return nDly; }          // D+0xd8
    float width() const { return wid; }              // D+0xc0
    float gain() const { return gn; }                // D+0x8c
    float progress() const { return prog; }          // D+0xe4
    float detuneTarget() const { return detT; }      // D+0xa4
    float widthTarget() const { return widT; }       // D+0xa8
    float level() const { return lev; }              // D+0xa0 (2nd: D+0x98)
    // D+0x90 HPF/Q, +0x94 LPF, +0x98 Sub (1st), +0xcc, +0xd0 the Own depths
    const float* slews() const { return sl; }
    const float* fractions() const { return fr; }    // D+0xdc, +0xe0
    float dc(int ch = 0) const { return dcy[ch]; }   // D+0x34, R's D+0x6c
    const float* state() const { return s; }         // D+0x1c…, R's from s[14] (D+0x54…)
    int writeIndex() const { return w; }             // D+4
    const float* buffer(int i) const {               // D+8, +0xc, R's +0x10, +0x14
        return i == 0 ? buf1.data() : i == 1 ? buf2.data() + kGuard : i == 2 ? buf3.data() : buf4.data() + kGuard;
    }

    // What the new block and note-on form, for the VC-2's adapter (v260; read only)
    float detuneRatio() const { return detR; }       // 1 − Detune/2400, the block copy's
    float subRatio() const { return subR; }          // 1 − Sub/2400 (1st)
    float hpfHalf() const { return hpfH; }           // HPF/2 (1st)
    float octaveFactor() const { return oct; }       // D+0x9c
    float dcRate() const { return dcK; }             // D+0x38, R's D+0x70

    // Line 2's taps reach up to kGuard words before its buffer.  The V-Synth's stay 0; the VC-2's are given what lies
    // there in rom.4's memory, before each block (v264; i 1, R's 3, as `buffer`)
    static constexpr int kGuard = 136;
    void setEarly(int i, const float* words) { std::copy_n(words, kGuard, (i == 1 ? buf2 : buf4).begin()); }

private:
    XtSbfBlock blk;
    float detR = 1.0f, subR = 1.0f, hpfH = 0;        // the block copy's 1 − Detune/2400, 1 − Sub/2400, HPF/2
    float pitch = 1.0f, root = 440.0f, oct = 2.0f;
    float detT = 1.0f, widT = 0, dly = 0, wid = 0, rate1 = 0, rate2 = 0;
    float nDly = 0, prog = 1.0f, gn = 0, lev = 0, dcy[2] = {}, dcK = 0;
    float sl[5] = {}, fr[2] = {}, s[20] = {};
    int w = 0;
    std::vector<float> buf1, buf2, buf3, buf4;       // buf2, buf4 after kGuard words that stay 0
};
