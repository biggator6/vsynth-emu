// SOLO grain player as a voice — see XtSoloVoice.h and the description in
// VariphraseEngine.cpp (soloFirmwareResynthOffline).
#include "XtSoloVoice.h"
#include "XtFloat.h"

#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>

// The grains, the filter and the gain are the SH-4's single precision, as XtMbPlayer's: plain
// float arithmetic under FE_TOWARDZERO (noteOn and the render set it), FMAC a product then a sum.
#pragma clang fp contract(off)
#pragma STDC FENV_ACCESS ON

namespace {
struct RoundToZero {   // XT_ROUND=nearest: the P-code emulator's, for validation
    int saved;
    RoundToZero() : saved(std::fegetround()) { if (!xtf::nearest()) std::fesetround(FE_TOWARDZERO); }
    ~RoundToZero() { std::fesetround(saved); }
};
struct RoundNearest {  // for XtBeat, which is in XtFloat: its sums' TwoSum needs this, as in the band player
    int saved;
    RoundNearest() : saved(std::fegetround()) { std::fesetround(FE_TONEAREST); }
    ~RoundNearest() { std::fesetround(saved); }
};
// rom.2 stores some values a unit off the nearest float
constexpr float K(uint32_t bits) { return std::bit_cast<float>(bits); }
// A sample in the firmware's units.  The clamp leaves a read at the last sample with fraction 0,
// so what it reads past the end is multiplied by 0.
inline float at(const std::vector<float>& x, int32_t i) { return i >= 0 && (size_t)i < x.size() ? x[(size_t)i] * 32768.0f : 0.0f; }
// One read (0x8950d106, 0x8950d1f8): FLOAT, FSUB, FMAC.
inline float readAt(const std::vector<float>& x, int32_t i, float f) { const float a = at(x, i); return (at(x, i + 1) - a) * f + a; }
constexpr double kUnlimited = 1e30;
// The anchor (player +0x28) is an int32 in 1/16 samples on the XT, and
// overflows as one: 2^32/16 samples round.
double wrapAnchor(double a) {
    constexpr double kRound = 268435456.0;
    return a >= kRound / 2 ? a - kRound : a < -kRound / 2 ? a + kRound : a;
}
}

// A record's length in samples; past the last record, the terminator's 16.
double XtSoloVoice::lenOf(size_t i) const { return i < p->size() ? p->segLen[i] : 16.0; }

// The repeat budget, (info & 0x1F8) << 6; 0x7E00 (all bits) means
// unlimited, as do the terminator and unknown info.
double XtSoloVoice::budgetOf(size_t i) const {
    if (i >= p->size() || p->info[i] < 0) return kUnlimited;
    const int b = (p->info[i] & 0x1F8) << 6;
    return b == 0x7E00 ? kUnlimited : (double)b;
}

// 0x8950d4c0: the next record starts where this one ends.  One reaching past
// the wave's end (the terminator) may repeat without limit.
void XtSoloVoice::loadNext() {
    if (seg < p->size()) ++seg;
    recStart = recEnd;
    recEnd = recStart + lenOf(seg);
    budget = recEnd > (double)p->in.size() ? kUnlimited : budgetOf(seg);
}

// 0x8950d530: the previous record ends where this one starts (at 0 at the
// earliest).
void XtSoloVoice::loadPrev() {
    if (seg > 0) --seg;
    recEnd = recStart;
    recStart = recEnd - lenOf(seg);
    if (recStart < 0.0) { recStart = 0.0; recEnd = lenOf(seg); }
    budget = budgetOf(seg);
}

// 0x8950d6f0: the budget one repeat costs at this anchor step and walk,
// x = (3 − 2a or 1.5 − 0.5a)·|walk|·2^29 >> 16, with the low end doubled
// down to 0 below 0x800.
double XtSoloVoice::repeatCost() const {
    const float a = (float)(int32_t)std::lround(anchorStep * 16.0) * 0.0625f;
    float w = (float)walkStep * 0x1p-25f;
    float x = 1.0f > a ? a * -2.0f + 3.0f : a * -0.5f + 1.5f;
    if (0.0f > x) x = 0.0f;
    if (0.0f > w) w = -w;
    x = x * w * 0x1p29f;
    int c = (int)((uint32_t)xtf::ftrc(x) >> 16);
    if (0x800 > c) { c = 2 * c - 0x800; if (c < 0) c = 0; }
    return (double)c;
}

// The anchor moving forward (0x8950cf12).  Past the record's end it wraps
// back to repeat the record, if the position has not passed the end and the
// budget allows (or the record ends the wave); else the next records load,
// skipping those the position has passed while their budget lasts.
void XtSoloVoice::anchorForward() {
    anchor = wrapAnchor(anchor + anchorStep);
    if (anchor < recEnd) return;
    bool wrap = false;
    if (recEnd > Pq && budget > 0.0) {
        if (budget >= kUnlimited) wrap = true;
        else { const double c = repeatCost(); if (budget >= c) { budget -= c; wrap = true; } }
    }
    if (!wrap && recEnd >= (double)p->in.size()) wrap = true;
    if (wrap) {
        // 0x8950cf4a: one wrap is anchor − length; more (a step longer than
        // the record) follow the firmware's loop
        double r1 = anchor - recEnd, r3 = recStart;
        bool again;
        do { r1 += r3; again = r1 >= recEnd; r3 -= recEnd; } while (again);
        anchor = r1;
        return;
    }
    for (;;) {
        loadNext();
        if (!(budget > 0.0) || !(Pq >= recEnd)) return;
        anchor = wrapAnchor(anchor + recEnd - recStart);
    }
}

// The anchor moving backward (0x8950cfee), the mirror image: below the
// record's start it wraps up to repeat it, or the previous records load.
void XtSoloVoice::anchorBackward() {
    anchor = wrapAnchor(anchor - anchorStep);
    if (!(recStart > anchor)) return;
    bool wrap = false;
    if (Pq >= recStart && budget > 0.0) {
        if (budget >= kUnlimited) wrap = true;
        else { const double c = repeatCost(); if (budget >= c) { budget -= c; wrap = true; } }
    }
    if (!wrap && 0.0 >= recStart) wrap = true;                      // the first record
    if (wrap) {
        // 0x8950d01a.  r3 falls by the start each round, so an anchor two
        // lengths under a start above the length comes back only by overflowing.
        double r1 = wrapAnchor(anchor - recStart), r3 = recEnd;
        bool again;
        do { r1 = wrapAnchor(r1 + r3); again = recStart >= r1; r3 = wrapAnchor(r3 - recStart); } while (again);
        anchor = r1;
        return;
    }
    for (;;) {
        loadPrev();
        if (!(budget > 0.0) || !(recStart > Pq)) return;
        anchor = wrapAnchor(anchor + recStart - recEnd);
    }
}

// The loop jump (0x8950d7d0, then 0x8950d4c0): position and anchor at the
// cached event, the record containing it loaded (the stream's terminator at
// the wave's end), event 0 current (a SOLO wave's events are its start and
// its end).  The running grains and the trigger timer carry on; landing in
// an unvoiced record clears the Energy filter's cutoff (+0x148), as an event
// jump does.
void XtSoloVoice::jumpTo(int32_t pos16) {
    Pi = pos16;
    Pf = 0;
    const double L = pos16 / 16.0;
    const size_t nc = p->size();
    size_t i = (size_t)(std::upper_bound(p->segStart.begin(), p->segStart.end(), L) - p->segStart.begin());
    i = i > 0 ? i - 1 : 0;
    if (i + 1 == nc && L >= p->segStart[i] + p->segLen[i]) i = nc;
    seg = i;
    recStart = i < nc ? p->segStart[i] : p->segStart[nc - 1] + p->segLen[nc - 1];
    recEnd = recStart + lenOf(i);
    budget = recEnd > (double)p->in.size() ? kUnlimited : budgetOf(i);
    anchor = L;
    if (i >= nc || !p->voiced[i]) f0n = 0.0f;
    curEv = 0;
    prevEv = 0;
    nextEv = endPos16;
    robotEv = robot;                    // an event's load (0x8950c7e0): +0x124 from byte 0's Robot bit
}

// The next crossing (0x8950ccc0, each block and after each crossing): the
// next event, and the loop points ahead in the walk's direction.  Walking
// back inside an Alternate bounce, only its far end counts.
void XtSoloVoice::crossings() {
    int32_t X;
    if (walkStep >= 0) {
        X = nextEv;
        if (f3 & 4) { if (X > lp[2]) X = lp[2]; nextCross = X; return; }
        if (lp[2] > Pi && X > lp[2]) X = lp[2];
        if (lmode1 > 0) {
            if (loopOn) { if (X > lp[1]) X = lp[1]; }
            else if (lp[1] > Pi && X > lp[1]) X = lp[1];
        }
    } else {
        X = prevEv;
        if (f3 & 8) { if (lp[0] > X) X = lp[0]; nextCross = X; return; }
        if (Pi > lp[0] && lp[0] > X) X = lp[0];
        if (lmode3 > 0) {
            if (loopOn) { if (lp[3] > X) X = lp[3]; }
            else if (Pi > lp[3] && lp[3] > X) X = lp[3];
        }
    }
    nextCross = X;
}

// Crossing forward (0x8950c880 for SOLO): the backward loop's end ends a
// bounce there; the forward loop's end loops (One Way: to the forward start;
// Alternate: the walk turns round; mode 3: to the forward start, holding);
// the next event (the wave's end) ends the note.  The loop regions' rate
// factors (+0xa0/+0xa4 into +0x9c): the pitch takes them inside the loop, at the next block.
void XtSoloVoice::crossForward() {
    const int32_t X = nextCross;
    if (lp[2] == X) {
        if (f3 & 4) { walkStep = -walkStep; f3 &= 0xfb; return; }
        if (f3 & 0x10) { f3 &= 0xef; f9c = 1.0f; }
    }
    if (lp[1] == X && !(f3 & 4)) {
        if (!loopOn) { f3 &= 0xdf; f9c = 1.0f; }
        else {
            f3 |= 0x20;
            f9c = loopCfg.fwdRate;
            ended_ = 0;
            if (lmode1 == 1) { jumpTo(lp[0]); return; }
            if (lmode1 == 2) { f3 |= 8; walkStep = -walkStep; return; }
            if (lmode1 == 3) { walkStep = 0; f3 |= 0x40; jumpTo(lp[0]); return; }
        }
    }
    if (nextEv != X) return;
    curEv = 1;                          // the end event, the last
    robotEv = robot;
    prevEv = nextEv;
    nextEv = endPos16 + 16;
    ended_ = 1;
}

// Crossing backward (0x8950ca70): the mirror image, at the forward start and
// the backward start (One Way: to the backward end; mode 3: holding there);
// event 0 (the wave's start) ends the note.
void XtSoloVoice::crossBackward() {
    const int32_t X = nextCross;
    if (lp[0] == X) {
        if (f3 & 8) { walkStep = -walkStep; f3 &= 0xf7; return; }
        if (f3 & 0x20) { f3 &= 0xdf; f9c = 1.0f; }
    }
    if (lp[3] == X && !(f3 & 8)) {
        if (loopOn) {
            f3 |= 0x10;
            f9c = loopCfg.bwdRate;
            ended_ = 0;
            if (lmode3 == 1) { jumpTo(lp[2]); return; }
            if (lmode3 == 2) { f3 |= 4; walkStep = -walkStep; return; }
            if (lmode3 == 3) { walkStep = 0; f3 |= 0x40; jumpTo(lp[3]); return; }
        }
        f3 &= 0xef;
        f9c = 1.0f;
    }
    if (prevEv != X) return;
    if (curEv > 0) { --curEv; robotEv = robot; prevEv = 0; nextEv = endPos16; return; }
    ended_ = -1;
}

// Grain parameters for a trigger at phrase position P in the current record
// (0x8950d5a0).  Returns the grain's start, velocity and second-tap weight;
// sets ppw, slope, the anchor step and (voiced only) f0n.
XtSoloVoice::Grain XtSoloVoice::grainParams(bool setF0) {
    const size_t nc = p->size(), sg = seg;
    const int32_t st16 = (int32_t)std::lround(recStart * 16.0), en16 = (int32_t)std::lround(recEnd * 16.0), len16 = en16 - st16;
    // Only the PERIOD is floored (160/16 samples); the record's extent, the
    // anchor wrap and the second-tap offset use the raw length.
    float C = (float)len16;
    if (160.0f > C) C = 160.0f;
    // After the last record the XT's stream holds a (256, 1) terminator: the
    // last record's next period is 16 samples, and the terminator's own is
    // its length (0x8950d4c0).
    float N = (float)(int32_t)std::lround(16.0 * lenOf(sg < nc ? sg + 1 : sg));
    if (160.0f > N) N = 160.0f;
    float frac = 0.0f, Ci = C;
    if (Pi > en16) { frac = 1.0f; Ci = N; }
    else if (Pi > st16) {
        frac = (float)(Pi - st16) / C;
        Ci = frac * (N - C) + C;
    }
    bool voiced = sg < nc && p->voiced[sg] != 0;                   // the terminator: info 1
    if (p->forceUnvoiced) voiced = false;
    if (p->forcedInfo >= 0) voiced = (p->forcedInfo & 4) != 0;
    const int info = p->forcedInfo >= 0 ? p->forcedInfo : sg < nc ? p->info[sg] : 1;
    const float Af = K(0x34ffffff) > rate98 ? K(0x34ffffff) : rate98, Ff = K(0x34ffffff) > F ? K(0x34ffffff) : F;
    float rate, v, pw;                                 // ppw divisor, read velocity, period
    // Robot voice: the pitch term (player +0x124 · A/256) is 0, so a voiced
    // record's grains come at a fixed period, A·440 Hz (1603.64 = 44100·16/440),
    // and an unvoiced one plays at rate 1.
    const float Ar = robotEv ? 0.0f : Af;
    if (voiced) {
        v = Ff;
        if (robotEv) { pw = K(0x44c8745d) / Af; rate = Ci / pw; }
        else { rate = Af; pw = Ci / rate; }
    } else {
        // Info bit 0x1 gates the pitch factor, 0x2 the formant factor: the
        // XT's unvoiced records (low bits 0) play at S = 1.  Unknown info
        // (a cuts file without it): S = A.
        rate = Ar == 0.0f ? 1.0f : Ar;
        if (info >= 0) rate = ((info & 1) ? rate : 1.0f) * ((info & 2) ? Ff : 1.0f);
        v = rate;
        pw = Ci / rate;
    }
    if (pw > 524287.0f) pw = 524287.0f;                // 1/16 samples
    float wl = Ci / v;
    int32_t os = 0;                                    // in whole 1/16 samples
    if (wl > pw) { os = xtf::ftrc(Ci - v * pw); wl = pw; }
    if (voiced && setF0) { f0n = 16.0f / pw; if (f0n > 0.5f) f0n = 0.5f; }
    ppw = xtf::ftrc(pw * 256.0f);                      // the timer's grid, 1/4096 samples
    slope = 8.0f / wl;                                 // 8/(16·wl) = 0.5/wl per sample
    // Anchor step (player +0x30) is stored in whole 1/16 samples:
    // trunc(16·rate), at least 16 — pitch +7 steps 23/16, not 1.498.
    float s = rate * 16.0f;
    if (16.0f > s) s = 16.0f;
    anchorStepInit = xtf::ftrc(s) / 16.0;
    return { st16 + os, v, frac, len16 };
}

void XtSoloVoice::noteOn(const XtSoloPlan& plan, double A_, double F_, double walk_, double energy_) {
    const RoundToZero round;
    p = &plan;
    A = (float)A_; F = (float)F_; walk = (float)walk_; energy = (float)energy_;
    f9c = 1.0f;
    rate98 = A;
    robotEv = robot;
    for (Chan& c : ch) c = Chan();
    next = 1;            // trigger toggle starts at 0 → first trigger → B
    slope = 0.0f;
    Pf = walkStep = 0;
    timer = ppw = 0;
    ended_ = 0;
    // Loops: the cached events (none: the forward end and the backward end
    // out of reach) and the walk flags as note-on leaves them (0x8950fba0),
    // event 0 current.
    endPos16 = (int32_t)p->in.size() * 16;
    loopOn = loopCfg.on;
    if (loopOn) {
        lp[0] = loopCfg.fwdStart * 16; lp[1] = loopCfg.fwdEnd * 16;
        lp[2] = loopCfg.bwdEnd * 16;   lp[3] = loopCfg.bwdStart * 16;
        lmode1 = loopCfg.fwdMode; lmode3 = loopCfg.bwdMode;
    } else {
        lp[0] = 0; lp[1] = lp[2] = 0x7fffffff; lp[3] = 0;
        lmode1 = lmode3 = 0;
    }
    f3 = 0x80;
    curEv = 0;
    prevEv = 0;
    nextEv = endPos16;
    nextCross = 0;
    // The start (0x8950fa60): a seek (0x8950fed0) to the wave's start, or to
    // its end when walk < 0: the position and the anchor there, and the
    // record from there on, record 0 or the stream's terminator (ending 16
    // samples past the wave's end, an unlimited budget).
    const double n = (double)p->in.size();
    if (walk < 0.0f) {
        seg = p->size();
        recStart = n; recEnd = n + lenOf(seg);
        budget = kUnlimited;
    } else {
        seg = 0;
        recStart = 0.0; recEnd = lenOf(0);
        budget = recEnd > n ? kUnlimited : budgetOf(0);
    }
    anchor = recStart;
    Pi = (int32_t)std::lround(recStart * 16.0);
    Pq = recStart;
    anchorStep = std::max((double)A, 1.0);
    low[0] = low[1] = band[0] = band[1] = fCoef = 0.0f;
    q = 1.2f;            // 0x3f99999a, where each block's target is a unit under it
    f0n = 0.0f;
    anchorStepInit = 1.0;
    t = 0;
    // Note-on (0x8950fba0): grain params at the event, timer and ages 0, both
    // channels reading from the record start, A's window at 0 and B's at 0.5
    // (the two sum to 1), f0n zeroed after the params are computed.
    const Grain g = grainParams(false);
    for (Chan& c : ch) { c.start = Pi; c.age = 0; c.v = g.v; c.tap = g.C; c.tapW = g.frac; }
    ch[0].e = 0.0f; ch[1].e = 0.5f;
    anchorStep = anchorStepInit;
    {
        const RoundNearest nearest;
        beat_.start(tempoCfg, origCfg, Pi);   // 0x8950f860
    }
}

// 0x8950fed0: as a loop jump's position and record (jumpTo), but the anchor at the record's
// start.  It loads event 0 first, whose first record is the one that can clear the Energy
// filter's cutoff.
void XtSoloVoice::seek(int32_t pos16) {
    if (!p || p->size() < 2) return;
    pos16 = pos16 < 0 ? 0 : pos16 > endPos16 ? endPos16 : pos16;
    const float keep = f0n;
    jumpTo(pos16);
    f0n = p->voiced[0] ? keep : 0.0f;
    anchor = recStart;
    beat_.reset((flags0 & 4) != 0);       // 0x8950f900
    beat_.beatPos = (int32_t)((uint32_t)pos16 << 5);
}

void XtSoloVoice::setRates(double A_, double F_, double walk_) { A = (float)A_; F = (float)F_; walk = (float)walk_; }

// 0x8950c280 for SOLO: walk·2^25 by the beat's factor, turned round inside an Alternate bounce
// until the walk itself turns; held at 0 after a mode-3 loop until the walk turns away from the loop.
void XtSoloVoice::walkBlock() {
    const float fs = [&] { const RoundNearest nearest; return beat_.block(flags0, walk, lp); }();
    if (!(f3 & 0x40)) {
        int32_t step = xtf::ftrc(fs);
        const bool neg = 0.0f > fs;
        if (f3 & 8) { if (neg) f3 &= 0xf7; else step = -step; }
        if (f3 & 4) { if (neg) step = -step; else f3 &= 0xf7; }   // (sic: 0x08)
        walkStep = step;
    } else {
        if ((f3 & 0x20) ? 0.0f > fs : fs > 0.0f) { f3 &= 0x8f; f9c = 1.0f; }
        walkStep = 0;
    }
}

// 0x8950cde0.  Energy g = 0 (OFF): cutoff f0/8, damping 1.2 (rumble filter).
// Energy g > 0: cutoff at the output fundamental, damping
// (1.2 − g) + p(rate) with p = g·(2r − 1) on [0.5, 1),
// g·(2 − r) on [1, 2), 0 outside.  Runs at block start, before
// the block's triggers, and reads the cutoff f0n that only voiced
// triggers write.
void XtSoloVoice::filterBlock() {
    rate98 = robot ? A : A * f9c;                     // 0x8950cde0
    const float k12 = K(0x3f999999);                  // 1.2, a unit under note-on's
    float x = f0n, qt;
    if (energy == 0.0f) { x = x * 0.125f; qt = k12; }
    else {
        float pr = 0.0f;
        if (!(0.5f > rate98)) {
            if (1.0f > rate98) pr = (2.0f * rate98 - 1.0f) * energy;
            else if (2.0f > rate98) pr = (2.0f - rate98) * energy;
        }
        qt = k12 - energy + pr;
    }
    const float ft = x * (K(0x40462824) - K(0x408d7731) * x * x) * 2.0f;   // 3.0962, 4.4208
    fCoef = 0.125f * (ft - fCoef) + fCoef;
    q = 0.125f * (qt - q) + q;
    const float lim = K(0x3ff33333) - q;              // 1.9
    if (fCoef > lim) fCoef = lim;
}

void XtSoloVoice::render(float* out, float* right, int count) {
    float lr[64];
    for (int done = 0; done < count; ) {
        const int n = std::min(count - done, 32);
        renderXt(lr, n);
        for (int i = 0; i < n; ++i) {
            out[done + i] = lr[2 * i] * 0x1p-31f;
            if (right) right[done + i] = lr[2 * i + 1] * 0x1p-31f;
        }
        done += n;
    }
}

void XtSoloVoice::renderXt(float* lr, int count) {
    if (!p || p->size() < 2) { std::fill(lr, lr + 2 * count, 0.0f); return; }
    const RoundToZero round;
    const bool st = p->stereo();
    const size_t nc = p->size();
    const int32_t endPos = (int32_t)p->in.size() * 16, last = (int32_t)p->in.size() - 1;
    const std::vector<float>& X = p->in, & Y = p->inR;
    for (int k = 0; k < count; ++k, ++t) {
        if ((t & 31) == 0) {   // per-block walk step [c280] and coefficient update [cde0]
            walkBlock();
            filterBlock();
            crossings();                // [ccc0]
        }
        // Walk and records (0x8950ce80).  The position moves by half the walk
        // step in 12.20; the record logic sees its whole-1/16 part.  At the
        // wave's end (+0x1c) or start (+0x18) it holds and triggers stop (+4
        // set); the two running grains play out.  Reaching the next crossing
        // runs the loop and event logic, which may jump or turn the walk.
        const int32_t s = walkStep >> 1, sum = Pf + s;
        int32_t np = Pi + (sum >> 20), nf = sum & 0xfffff;
        if (s > 0) {
            if (np >= endPos) { ended_ = 1; np = endPos; nf = 0; }
            Pi = np; Pf = nf;
            if (nextCross <= Pi) { crossForward(); crossings(); }
        } else if (s < 0) {
            if (0 >= np) { ended_ = -1; np = 0; nf = 0; }      // +0x18: the start, 0
            Pi = np; Pf = nf;
            if (Pi <= nextCross) { crossBackward(); crossings(); }
        }
        Pq = Pi / 16.0;
        if (!p->anchorMode) {           // diagnostic: the record containing P
            while (seg < nc && Pq >= recEnd) loadNext();
            while (seg > 0 && Pq < recStart) loadPrev();
        } else if (s > 0) anchorForward();
        else if (s < 0) anchorBackward();
        else if (Pq >= anchor) anchorForward();     // holding: the anchor closes on P from either side
        else anchorBackward();
        // The timer (grain +0xc) is an int32 in 1/4096 samples.  With a period
        // under a sample it gains on every trigger until it overflows; the
        // triggers then wait for it to climb back to the period.
        timer = (int32_t)((uint32_t)timer + 0x1000u);
        if (timer >= ppw) {
            timer -= ppw;
            if (!ended_) {
                const Grain g = grainParams(true);
                anchorStep = anchorStepInit;
                Chan& c  = ch[next];
                next ^= 1;
                c.start  = g.start;
                c.age    = timer;
                c.v      = g.v;
                c.e      = (float)timer * 0x1p-12f * slope;
                c.tap    = g.C;
                c.tapW   = g.frac;
            }
        }

        if (trace)   // VSE_TRACE: same fields/units as EmuSolo.java's trace
            std::fprintf(trace, "%ld\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%d\t%d\n",
                t, (double)Pi, std::floor(anchor * 16), recStart * 16, recEnd * 16,
                (recEnd - recStart) * 16, timer, ppw, ch[0].age, ch[1].age,
                ch[0].start, ch[1].start, next ^ 1, ch[0].e, ch[1].e,
                ch[0].v, ch[1].v, ch[0].tapW, ch[1].tapW, slope, ch[0].tap, ch[1].tap);

        // The windows and the read positions (0x8950d380): each grain's window e clamped to 1,
        // w = 2·min(e, 1 − e), e + the shared slope; its position start + age·v, age + 1 sample.
        float w[2], fr[2];
        int32_t idx[2];
        for (int g = 0; g < 2; ++g) {
            Chan& c = ch[g];
            float e = c.e;
            if (e > 1.0f) e = 1.0f;
            w[g] = (0.5f > e ? e : -e + 1.0f) * 2.0f;
            c.e = e + slope;
            const float x = (float)c.age * c.v * 0x1p-12f + 0.0625f * (float)(c.start & 15);
            c.age = (int32_t)((uint32_t)c.age + 0x1000u);
            const int32_t whole = xtf::ftrc(x);
            idx[g] = (int32_t)((uint32_t)c.start >> 4) + whole;
            fr[g] = x - (float)whole;
            if (!(last > idx[g])) { idx[g] = last; fr[g] = 0.0f; }      // 0x8950d420
        }
        // The reads (0x8950d0f0; 0x8950d1f0 for a stereo wave, at the same indices and fractions)
        float a[2] = { readAt(X, idx[0], fr[0]), readAt(X, idx[1], fr[1]) };
        float b[2] = { 0.0f, 0.0f };
        if (st) { b[0] = readAt(Y, idx[0], fr[0]); b[1] = readAt(Y, idx[1], fr[1]); }
        // The second tap, one record period on: both grains or neither, on the sum of their weights.
        // The second grain's weight is that sum less the first's, which can lose its last bit.
        const float tapA = ch[0].tapW, tapSum = ch[1].tapW + tapA, tapB = tapSum - tapA;
        if (tapSum > 0.0f) {
            for (int g = 0; g < 2; ++g) {
                const float x = 0.0625f * (float)ch[g].tap + fr[g];
                const int32_t whole = xtf::ftrc(x);
                idx[g] += whole;
                fr[g] = x - (float)whole;
                if (!(last > idx[g])) { idx[g] = last; fr[g] = 0.0f; }
            }
            a[0] = tapA * (readAt(X, idx[0], fr[0]) - a[0]) + a[0];
            a[1] = tapB * (readAt(X, idx[1], fr[1]) - a[1]) + a[1];
            if (st) {
                b[0] = tapA * (readAt(Y, idx[0], fr[0]) - b[0]) + b[0];
                b[1] = tapB * (readAt(Y, idx[1], fr[1]) - b[1]) + b[1];
            }
        }
        // Highpass SVF, one per channel with shared coefficients (+0x54/+0x58
        // left, +0x5c/+0x60 right), then the gain (0x8950d1c2): gain += (target − gain)/64, ×65536.
        const float y = a[0] * w[0] + a[1] * w[1];
        low[0] = fCoef * band[0] + low[0];
        const float high = y - q * band[0] - low[0];
        band[0] = fCoef * high + band[0];
        gain = 0.015625f * (gainTarget - gain) + gain;
        const float g65536 = gain * 65536.0f;
        lr[2 * k] = lr[2 * k + 1] = high * g65536;
        if (st) {
            const float yR = b[0] * w[0] + b[1] * w[1];
            low[1] = fCoef * band[1] + low[1];
            const float highR = yR - q * band[1] - low[1];
            band[1] = fCoef * highR + band[1];
            lr[2 * k + 1] = highR * g65536;
        }
    }
}

#ifdef XT_SOLO_MAIN
// xt_solo <cuts.txt> <samp.bin int16> <channels> <R> <F> <walk> [energy] [maxOut] <out.f32>
// The same inputs as EmuSolo.java (cuts.txt: "start len voiced info" lines,
// as emu_inputs.py writes them); output scaled to ±1, L/R pairs for 2 channels.
#include <cstdlib>
#include <cstring>
int main(int argc, char** argv) {
    if (argc < 8) { std::fprintf(stderr, "usage: xt_solo cuts.txt samp.bin channels R F walk [energy] [maxOut] out.f32\n"); return 2; }
    const int chans = std::atoi(argv[3]);
    const double R = std::atof(argv[4]), F = std::atof(argv[5]), walk = std::atof(argv[6]);
    const double energy = argc > 8 ? std::atof(argv[7]) : 0.0;
    const long maxOut = argc > 9 ? std::atol(argv[8]) : 0;
    XtSoloPlan plan;
    FILE* f = std::fopen(argv[2], "rb");
    if (!f) return 1;
    std::vector<int16_t> pcm;
    int16_t s;
    while (std::fread(&s, 2, 1, f) == 1) pcm.push_back(s);
    std::fclose(f);
    const size_t n = pcm.size() / (size_t)chans;
    plan.in.resize(n);
    if (chans == 2) plan.inR.resize(n);
    for (size_t i = 0; i < n; ++i) {
        plan.in[i] = pcm[i * chans] / 32768.0f;
        if (chans == 2) plan.inR[i] = pcm[i * 2 + 1] / 32768.0f;
    }
    if (!(f = std::fopen(argv[1], "r"))) return 1;
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
        double st, len; int v, info = -1;
        if (std::sscanf(line, "%lf %lf %d %d", &st, &len, &v, &info) < 3) continue;
        plan.segStart.push_back(st); plan.voiced.push_back((char)(v != 0)); plan.info.push_back(info);
    }
    std::fclose(f);
    const size_t nc = plan.size();
    plan.segLen.resize(nc);
    for (size_t i = 0; i < nc; ++i) {
        const double end = (i + 1 < nc) ? plan.segStart[i + 1] : (double)n;
        plan.segLen[i] = std::max(1.0 / 16.0, end - plan.segStart[i]);
    }
    const long outLen = maxOut > 0 ? maxOut : std::max(1L, std::lround((double)n / walk));
    std::vector<float> L((size_t)outLen), Rt((size_t)outLen);
    XtSoloVoice voice;
    if (const char* tf = std::getenv("VSE_TRACE")) voice.trace = std::fopen(tf, "w");
    voice.noteOn(plan, R, F, walk, energy);
    voice.render(L.data(), Rt.data(), (int)outLen);
    if (!(f = std::fopen(argv[argc - 1], "wb"))) return 1;
    for (long i = 0; i < outLen; ++i) {
        std::fwrite(&L[(size_t)i], 4, 1, f);
        if (chans == 2) std::fwrite(&Rt[(size_t)i], 4, 1, f);
    }
    std::fclose(f);
    if (voice.trace) std::fclose(voice.trace);
    std::printf("%ld frames, %zu records\n", outLen, nc);
    return 0;
}
#endif
