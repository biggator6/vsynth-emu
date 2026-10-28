// SOLO grain player as a voice — see XtSoloVoice.h and the description in
// VariphraseEngine.cpp (soloFirmwareResynthOffline).
#include "XtSoloVoice.h"

#include <algorithm>
#include <cmath>

double XtSoloVoice::at(int i) const {
    const int n = (int)p->in.size();
    return (i >= 0 && i < n) ? (double)p->in[(size_t)i] : 0.0;
}

// Sample reads (0x8950d380 / d0f0): the grain's read index is clamped to the
// last sample (fraction 0); the second tap, one record period further on, is
// not clamped and reads silence past the end.
double XtSoloVoice::readGrain(double pos, double tap, double tapW) const {
    const int n = (int)p->in.size();
    int i0 = (int)std::floor(pos);
    double fr = pos - i0;
    if (i0 >= n - 1) { i0 = n - 1; fr = 0.0; }
    double s = at(i0) + (at(i0 + 1) - at(i0)) * fr;
    if (tapW > 0.0) {
        const double p2 = fr + tap;
        const int j = i0 + (int)std::floor(p2);
        const double f2 = p2 - std::floor(p2);
        s += (at(j) + (at(j + 1) - at(j)) * f2 - s) * tapW;
    }
    return s;
}

double XtSoloVoice::budgetOf(size_t i) const {
    if (i + 1 >= p->size() || p->info[i] < 0) return 1e30;          // unlimited
    return (double)((p->info[i] & 0x1F8) << 6);
}

double XtSoloVoice::repeatCost(double a, double tw) {
    double x = (a < 1.0 ? (3.0 - 2.0 * a) : (1.5 - 0.5 * a));
    x = std::max(0.0, x) * std::abs(tw) * 8192.0;
    return x >= 2048.0 ? x : std::max(0.0, 2.0 * x - 2048.0);
}

// Grain parameters for a trigger at phrase position P in record sg
// (0x8950d5a0).  Returns the grain's start, velocity and second-tap weight;
// sets ppw, slope, the anchor step and (voiced only) f0n.
XtSoloVoice::Grain XtSoloVoice::grainParams(size_t sg, bool setF0) {
    const size_t nc = p->size();
    // Only the PERIOD is floored (160/16 samples); the record's extent, the
    // anchor wrap and the second-tap offset use the raw length.
    const double C = std::max(10.0, p->segLen[sg]);
    // After the last record the XT's stream holds a (256, 1) terminator: the
    // last record's next period is 16 samples.
    const double N = std::max(10.0, (sg + 1 < nc) ? p->segLen[sg + 1] : 16.0);
    const double frac = (Pq > p->segStart[sg] + p->segLen[sg]) ? 1.0
                      : (Pq > p->segStart[sg]) ? (Pq - p->segStart[sg]) / C : 0.0;
    const double Ci = C + frac * (N - C);
    bool voiced = p->voiced[sg] != 0;
    if (p->forceUnvoiced) voiced = false;
    if (p->forcedInfo >= 0) voiced = (p->forcedInfo & 4) != 0;
    double v, wl, os = 0.0, pw;
    if (voiced) {
        pw  = std::min(Ci / A, 524287.0 / 16.0);
        v   = F;
        wl  = Ci / F;
        // os is stored in whole 1/16 samples (truncated)
        if (wl > pw) { os = std::trunc(16.0 * (Ci - pw * F)) / 16.0; wl = pw; }
        if (setF0) f0n = std::min(1.0 / pw, 0.5);
    } else {
        double S = A;
        if (p->forcedInfo >= 0) S = ((p->forcedInfo & 1) ? A : 1.0) * ((p->forcedInfo & 2) ? F : 1.0);
        pw  = std::min(Ci / S, 524287.0 / 16.0);
        v   = S;
        wl  = pw;
    }
    // The trigger timer runs in 1/4096 samples against ppw truncated to that
    // grid (grain +4); unrounded ppw drifts a trigger by a sample within
    // ~1700 samples on the XT sine.
    ppw   = std::floor(pw * 4096.0) / 4096.0;
    slope = 0.5 / wl;
    // Anchor step (player +0x30) is stored in whole 1/16 samples:
    // trunc(16·rate), at least 16 — pitch +7 steps 23/16, not 1.498.
    anchorStepInit = std::max(std::floor(16.0 * (voiced ? A : v)), 16.0) / 16.0;
    return { p->segStart[sg] + os, v, frac, p->segLen[sg] };
}

void XtSoloVoice::noteOn(const XtSoloPlan& plan, double A_, double F_, double stretch, double energy_) {
    p = &plan;
    A = A_; F = F_; walk = 1.0 / stretch; energy = energy_;
    for (Chan& c : ch) c = Chan();
    next = 1;            // trigger toggle starts at 0 → first trigger → B
    slope = 0.0;
    P = Pq = timer = ppw = 0.0;
    ended_ = false;
    seg = 0;
    anchor = 0.0;
    anchorStep = std::max(A, 1.0);
    budget = budgetOf(0);
    low = band = fCoef = 0.0;
    q = 1.2;
    f0n = 0.0;
    anchorStepInit = 1.0;
    t = 0;
    // Note-on (0x8950fba0): grain params at P = 0, timer and ages 0, both
    // channels reading from the record start, A's window at 0 and B's at 0.5
    // (the two sum to 1), f0n zeroed after the params are computed.
    const Grain g = grainParams(0, false);
    for (Chan& c : ch) { c.start = p->segStart[0]; c.age = 0; c.v = g.v; c.tap = g.C; c.tapW = g.frac; }
    ch[0].e = 0.0; ch[1].e = 0.5;
    anchorStep = anchorStepInit;
}

void XtSoloVoice::setRates(double A_, double F_, double stretch) { A = A_; F = F_; walk = 1.0 / stretch; }

void XtSoloVoice::render(float* out, int count) {
    if (!p || p->size() < 2) { std::fill(out, out + count, 0.0f); return; }
    const size_t nc = p->size();
    const double n = (double)p->in.size();
    for (int k = 0; k < count; ++k, ++t) {
        if ((t & 31) == 0) {   // per-block coefficient update        [cde0]
            // Energy g = 0 (OFF): cutoff f0/8, damping 1.2 (rumble filter).
            // Energy g > 0: cutoff at the output fundamental, damping
            // (1.2 − g) + p(rate) with p = g·(2r − 1) on [0.5, 1),
            // g·(2 − r) on [1, 2), 0 outside.  Runs at block start, before
            // the block's triggers, and reads the cutoff f0n that only voiced
            // triggers write.
            const double r = A;
            double x = f0n, qt;
            if (energy <= 0.0) { x *= 0.125; qt = 1.2; }
            else {
                double pr = 0.0;
                if (r >= 0.5 && r < 1.0)      pr = (2.0 * r - 1.0) * energy;
                else if (r >= 1.0 && r < 2.0) pr = (2.0 - r) * energy;
                qt = (1.2 - energy) + pr;
            }
            const double ft = x * (3.0962 - 4.4208 * x * x) * 2.0;
            q     = (qt - q) * 0.125 + q;
            fCoef = (ft - fCoef) * 0.125 + fCoef;
            fCoef = std::min(fCoef, 1.9 - q);
        }
        P += walk;
        // Record logic sees only the whole-1/16 part of P (player +0x38).
        Pq = std::floor(P * 16.0) / 16.0;
        // End of the wave (+0x1c): position holds, no further triggers (+4
        // set); the two running grains play out.
        if (Pq >= n) { P = Pq = n; ended_ = true; }
        if (!p->anchorMode) {
            while (seg + 1 < nc && Pq >= p->segStart[seg + 1]) ++seg;
        } else {
            anchor += anchorStep;
            const double segEnd = p->segStart[seg] + p->segLen[seg];
            if (anchor >= segEnd) {
                const double cost = repeatCost(anchorStep, walk);
                if (Pq < segEnd && budget > 0.0 && budget >= cost) {
                    if (budget < 1e29) budget -= cost;
                    while (anchor >= p->segStart[seg] + p->segLen[seg]) anchor -= p->segLen[seg];
                } else if (seg + 1 < nc) {
                    ++seg; budget = budgetOf(seg);
                    while (seg + 1 < nc && budget > 0.0 && Pq >= p->segStart[seg] + p->segLen[seg]) {
                        anchor += p->segLen[seg];
                        ++seg; budget = budgetOf(seg);
                    }
                }
            }
        }
        timer += 1.0;
        const bool due = timer >= ppw;
        if (due) timer -= ppw;
        if (due && !ended_) {
            const Grain g = grainParams(seg, true);
            anchorStep = anchorStepInit;
            Chan& c  = ch[next];
            next ^= 1;
            c.start  = g.start;
            c.age    = timer;
            c.v      = g.v;
            c.e      = timer * slope;
            c.tap    = g.C;
            c.tapW   = g.frac;
        }

        if (trace)   // VSE_TRACE: same fields/units as EmuSolo.java's trace
            std::fprintf(trace, "%ld\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%.0f\t%d\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.0f\t%.0f\n",
                t, std::floor(P * 16), std::floor(anchor * 16), p->segStart[seg] * 16, (p->segStart[seg] + p->segLen[seg]) * 16,
                p->segLen[seg] * 16, timer * 4096, ppw * 4096, ch[0].age * 4096, ch[1].age * 4096,
                std::floor(ch[0].start * 16), std::floor(ch[1].start * 16), next ^ 1, ch[0].e, ch[1].e,
                ch[0].v, ch[1].v, ch[0].tapW, ch[1].tapW, slope, ch[0].tap * 16, ch[1].tap * 16);

        double y = 0.0;
        for (Chan& c : ch) {
            const double e = std::min(c.e, 1.0);
            const double w = 2.0 * std::min(e, 1.0 - e);
            if (w > 0.0) y += w * readGrain(c.start + c.age * c.v, c.tap, c.tapW);
            c.e = e + slope;
            c.age += 1.0;
        }

        low += fCoef * band;
        const double high = y - q * band - low;
        band += fCoef * high;
        out[k] = (float)high;
    }
}
