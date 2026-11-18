#pragma once
// Lo-Fi, XtCosm's COSM type 15 (block code 19): its parameters, block and object, moved from XtCosm.h as they stood
// (v253); XtCosm.h includes this, and its code stays in XtCosm.cpp.

// Lo-Fi (COSM Type 15, block code 19): an anti-alias lowpass, a sample hold,
// a bit quantizer and a lowpass after, mixed with the dry input by Balance.
// Parameters 0x941…0x949: Sample Bit Down 0…15 (10), Sample Rate Down 0…5
// (4), Filter 0…127 (64) with Keyfollow ±20, LFO and Env depths ±63,
// Dry/Effect Balance 0…100 (50) with LFO and Env depths.  Filter takes Env A
// (Env 9, COSM 2 Env 11) and its velocity factor, Balance Env B (10, 12);
// both LFO 3 (4).
struct XtLofiParams {
    int bitDown = 10, rateDown = 4, filter = 64, filterKf = 0, filterLfo = 0, filterEnv = 0, balance = 50,
        balanceLfo = 0, balanceEnv = 0;
};

// The converter's block (0x894ddf80): the Bit and Rate Down as bytes; Filter
// through 0x894dc810, (Filter·0.9/127 + 0.1)·2^(KF·10·(key − 60)/1200),
// within 0.1…1; Balance/100; LFO depths (d/63)·|d/63|; Env depths A (Filter)
// or B (Balance) × (d/63)·|d/63|.
struct XtLofiBlock {
    int bitDown = 10, rateDown = 4;
    float filter = 0.1f, filterEnv = 0, filterLfo = 0, balance = 0.5f, balanceEnv = 0, balanceLfo = 0;
};
XtLofiBlock xtLofiBlock(const XtLofiParams& p, int key, float velA, float velB);

// The object for block code 19 (0x894fe9c0).  Filter' = Filter + depths ×
// Env A and the LFO, within 0.1…1; Balance' = a table (rom.2, 128 entries) at
// trunc(127 × (Balance + depths, within 0…1)) (start values 0x894eca70;
// targets from the block 0x894edd60 and the update 0x894eeee0).  Per block
// both step toward their targets by rate·(target − value) before every fourth
// sample.  Per sample: two two-pole lowpasses at 1/2^Rate; the hold takes the
// second's output in the four-sample groups whose count (0, 4, … 28) has no
// bit of the Rate's mask (0, 1, 3, 7, 15, 31), so Rate Down 1 and 2 never
// hold (a firmware quirk); q = trunc(hold · 2^−Bit · 2^−15), on the XT's
// scale; a two-pole lowpass at Filter' over q · 2^Bit · 32768; out =
// Balance' × it + (1 − Balance')·x, on L and R from L.
class XtCosmLofi {
public:
    // Note-on (0x894edf70, no case of its own): states cleared, start values,
    // rates, then the block with the envelopes' outputs.
    void noteOn(const XtLofiBlock& b, float env1Start, float env1Out, float rate1, float lfo1,
                float env2Start, float env2Out, float rate2, float lfo2);
    void setBlock(const XtLofiBlock& b, float env1, float lfo1, float env2, float lfo2);     // 0x894edd60
    void update(float env1, float rate1, float lfo1, float env2, float rate2, float lfo2);    // 0x894eeee0
    // 32 frames, interleaved L/R, in place: reads L, writes both.  `scale`:
    // the buffer's units against the XT's (int16 × 65536), for the quantizer.
    // `stereo`: 0x894febe0.
    void process(float* lr, float scale = 1.0f, bool stereo = false);

    // The state, for the firmware checks (D = object + 0x58).
    float filter() const { return flt_; }            // D+0xbc
    float balance() const { return bal; }            // D+0xc0
    float filterTarget() const { return fltT; }      // D+0xa4
    float balanceTarget() const { return balT; }     // D+0xa8
    const float* state() const { return s; }         // D+0x1c…0x30: the three lowpasses' a, b; R's from s[14] (D+0x54…)
    // What the new block forms from the Rate and Bit Down, for the VC-2's adapter (v253; read only)
    int rateMask() const { return mask; }            // D+1
    float rateCoef() const { return coef; }          // D+0x94: 1/2^Rate
    float upScale() const { return up; }             // D+0x98: 2^Bit
    float downScale() const { return down; }         // D+0x9c: 2^−Bit

private:
    XtLofiBlock blk;
    float flt_ = 0.1f, bal = 0, fltT = 0.1f, balT = 0, rate1 = 0, rate2 = 0;
    float coef = 1.0f, up = 1.0f, down = 1.0f;       // 1/2^Rate, 2^Bit, 2^−Bit (D+0x94, +0x98, +0x9c)
    int mask = 0;                                    // D+1
    float s[20] = {};
    float filterOf(float env, float lfo) const;
    float balanceOf(float env, float lfo) const;
};
