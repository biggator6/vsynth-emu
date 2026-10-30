// SOLO grain player as a voice — see XtSoloVoice.h and the description in
// VariphraseEngine.cpp (soloFirmwareResynthOffline).
#include "XtSoloVoice.h"
#include "XtFloat.h"

#include <algorithm>
#include <cmath>

// Sample reads (0x8950d380 / d0f0): the grain's read index is clamped to the
// last sample (fraction 0); so is the second tap, one record period further
// on (0x8950d420 again).  A stereo wave reads both channels at the same
// indices and fractions (packed L/R frames, 0x8950d1f0).
double XtSoloVoice::readGrain(const std::vector<float>& x, double pos, double tap, double tapW) const {
    const int n = (int)x.size();
    auto at = [&](int i) { return (i >= 0 && i < n) ? (double)x[(size_t)i] : 0.0; };
    int i0 = (int)std::floor(pos);
    double fr = pos - i0;
    if (i0 >= n - 1) { i0 = n - 1; fr = 0.0; }
    double s = at(i0) + (at(i0 + 1) - at(i0)) * fr;
    if (tapW > 0.0) {
        const double p2 = fr + tap;
        int j = i0 + (int)std::floor(p2);
        double f2 = p2 - std::floor(p2);
        if (j >= n - 1) { j = n - 1; f2 = 0.0; }
        s += (at(j) + (at(j + 1) - at(j)) * f2 - s) * tapW;
    }
    return s;
}

namespace { constexpr double kUnlimited = 1e30; }

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
    using namespace xtf;
    const float a = mul(flt((int)std::lround(anchorStep * 16.0)), 0.0625f);
    float w = mul(flt(walkStep), 2.98023224e-8f);                   // 2^-25
    float x = 1.0f > a ? add(mul(a, -2.0f), 3.0f) : add(mul(a, -0.5f), 1.5f);
    if (0.0f > x) x = 0.0f;
    if (0.0f > w) w = -w;
    x = mul(mul(x, w), 536870912.0f);                                // 2^29
    int c = (int)((uint32_t)ftrc(x) >> 16);
    if (0x800 > c) { c = 2 * c - 0x800; if (c < 0) c = 0; }
    return (double)c;
}

// The anchor moving forward (0x8950cf12).  Past the record's end it wraps
// back to repeat the record, if the position has not passed the end and the
// budget allows (or the record ends the wave); else the next records load,
// skipping those the position has passed while their budget lasts.
void XtSoloVoice::anchorForward() {
    anchor += anchorStep;
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
        anchor += recEnd - recStart;
    }
}

// The anchor moving backward (0x8950cfee), the mirror image: below the
// record's start it wraps up to repeat it, or the previous records load.
void XtSoloVoice::anchorBackward() {
    anchor -= anchorStep;
    if (!(recStart > anchor)) return;
    bool wrap = false;
    if (Pq >= recStart && budget > 0.0) {
        if (budget >= kUnlimited) wrap = true;
        else { const double c = repeatCost(); if (budget >= c) { budget -= c; wrap = true; } }
    }
    if (!wrap && 0.0 >= recStart) wrap = true;                      // the first record
    if (wrap) {
        double r1 = anchor - recStart, r3 = recEnd;                  // 0x8950d01a
        bool again;
        do { r1 += r3; again = recStart >= r1; r3 -= recStart; } while (again);
        anchor = r1;
        return;
    }
    for (;;) {
        loadPrev();
        if (!(budget > 0.0) || !(recStart > Pq)) return;
        anchor += recStart - recEnd;
    }
}

// Grain parameters for a trigger at phrase position P in the current record
// (0x8950d5a0).  Returns the grain's start, velocity and second-tap weight;
// sets ppw, slope, the anchor step and (voiced only) f0n.  Computed as the
// firmware does, in single precision on the 1/16-sample grid: the truncated
// results (os, ppw, the anchor step) differ by a grid step in double when the
// exact value lands on the grid (os = 5365·(1 − 0.8) = 1073).
XtSoloVoice::Grain XtSoloVoice::grainParams(bool setF0) {
    const size_t nc = p->size(), sg = seg;
    // Only the PERIOD is floored (160/16 samples); the record's extent, the
    // anchor wrap and the second-tap offset use the raw length.
    const float C = std::max(160.0f, (float)(16.0 * (recEnd - recStart)));
    // After the last record the XT's stream holds a (256, 1) terminator: the
    // last record's next period is 16 samples, and the terminator's own is
    // its length (0x8950d4c0).
    const float N = std::max(160.0f, (float)(16.0 * lenOf(sg < nc ? sg + 1 : sg)));
    float frac = 0.0f, Ci = C;
    if (Pq > recEnd) { frac = 1.0f; Ci = N; }
    else if (Pq > recStart) {
        frac = xtf::div((float)(16.0 * (Pq - recStart)), C);
        Ci = xtf::add(xtf::mul(frac, xtf::sub(N, C)), C);          // fmac: product, then sum
    }
    bool voiced = sg < nc && p->voiced[sg] != 0;                   // the terminator: info 1
    if (p->forceUnvoiced) voiced = false;
    if (p->forcedInfo >= 0) voiced = (p->forcedInfo & 4) != 0;
    const int info = p->forcedInfo >= 0 ? p->forcedInfo : sg < nc ? p->info[sg] : 1;
    const float Af = std::max((float)A, 4.76837e-07f), Ff = std::max((float)F, 4.76837e-07f);
    float rate, v;                                     // ppw divisor, read velocity
    if (voiced) { rate = Af; v = Ff; }
    else {
        // Info bit 0x1 gates the pitch factor, 0x2 the formant factor: the
        // XT's unvoiced records (low bits 0) play at S = 1.  Unknown info
        // (a cuts file without it): S = A.
        rate = Af;
        if (info >= 0) rate = xtf::mul((info & 1) ? Af : 1.0f, (info & 2) ? Ff : 1.0f);
        v = rate;
    }
    const float pw = std::min(xtf::div(Ci, rate), 524287.0f);   // 1/16 samples
    float wl = xtf::div(Ci, v), os = 0.0f;
    if (wl > pw) { os = std::trunc(xtf::sub(Ci, xtf::mul(pw, v))); wl = pw; }   // os in whole 1/16 samples
    if (voiced && setF0) f0n = std::min(xtf::div(16.0f, pw), 0.5f);
    // The trigger timer runs in 1/4096 samples against ppw truncated to that
    // grid (grain +4); unrounded ppw drifts a trigger by a sample within
    // ~1700 samples on the XT sine.
    ppw   = std::trunc(xtf::mul(pw, 256.0f)) / 4096.0;
    slope = xtf::div(8.0f, wl);                              // 8/(16·wl) = 0.5/wl per sample
    // Anchor step (player +0x30) is stored in whole 1/16 samples:
    // trunc(16·rate), at least 16 — pitch +7 steps 23/16, not 1.498.
    anchorStepInit = std::trunc(std::max(xtf::mul(rate, 16.0f), 16.0f)) / 16.0;
    return { recStart + os / 16.0, v, frac, recEnd - recStart };
}

void XtSoloVoice::noteOn(const XtSoloPlan& plan, double A_, double F_, double walk_, double energy_) {
    p = &plan;
    A = A_; F = F_; walk = walk_; energy = energy_;
    for (Chan& c : ch) c = Chan();
    next = 1;            // trigger toggle starts at 0 → first trigger → B
    slope = 0.0;
    Pf = walkStep = 0;
    timer = ppw = 0.0;
    ended_ = 0;
    // The event jump (0x8950d7b0) at the start event, or at the end event
    // when walk < 0 (0x8950fa60): the position and the anchor at the event,
    // and the record it points at, record 0 or the stream's terminator
    // (ending 16 samples past the wave's end, an unlimited budget).
    const double n = (double)p->in.size();
    if (walk < 0.0) {
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
    anchorStep = std::max(A, 1.0);
    low[0] = low[1] = band[0] = band[1] = fCoef = 0.0;
    q = 1.2;
    f0n = 0.0;
    anchorStepInit = 1.0;
    t = 0;
    // Note-on (0x8950fba0): grain params at the event, timer and ages 0, both
    // channels reading from the record start, A's window at 0 and B's at 0.5
    // (the two sum to 1), f0n zeroed after the params are computed.
    const Grain g = grainParams(false);
    for (Chan& c : ch) { c.start = recStart; c.age = 0; c.v = g.v; c.tap = g.C; c.tapW = g.frac; }
    ch[0].e = 0.0; ch[1].e = 0.5;
    anchorStep = anchorStepInit;
}

void XtSoloVoice::setRates(double A_, double F_, double walk_) { A = A_; F = F_; walk = walk_; }

void XtSoloVoice::render(float* out, float* right, int count) {
    if (!p || p->size() < 2) {
        std::fill(out, out + count, 0.0f);
        if (right) std::fill(right, right + count, 0.0f);
        return;
    }
    const bool st = right && p->stereo();
    const size_t nc = p->size();
    const int32_t endPos = (int32_t)p->in.size() * 16;
    for (int k = 0; k < count; ++k, ++t) {
        if ((t & 31) == 0) {   // per-block walk step [c280] and coefficient update [cde0]
            walkStep = xtf::ftrc(xtf::mul((float)walk, 33554432.0f));   // walk·2^25
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
        // Walk and records (0x8950ce80).  The position moves by half the walk
        // step in 12.20; the record logic sees its whole-1/16 part.  At the
        // wave's end (+0x1c) or start (+0x18) it holds and triggers stop (+4
        // set); the two running grains play out.
        const int32_t s = walkStep >> 1, sum = Pf + s;
        int32_t np = Pi + (sum >> 20), nf = sum & 0xfffff;
        if (s > 0 && np >= endPos) { ended_ = 1; np = endPos; nf = 0; }
        if (s < 0 && 0 >= np) { ended_ = -1; np = 0; nf = 0; }
        Pi = np; Pf = nf;
        Pq = Pi / 16.0;
        if (!p->anchorMode) {           // diagnostic: the record containing P
            while (seg < nc && Pq >= recEnd) loadNext();
            while (seg > 0 && Pq < recStart) loadPrev();
        } else if (s > 0) anchorForward();
        else if (s < 0) anchorBackward();
        else if (Pq >= anchor) anchorForward();     // holding: the anchor closes on P from either side
        else anchorBackward();
        timer += 1.0;
        const bool due = timer >= ppw;
        if (due) timer -= ppw;
        if (due && !ended_) {
            const Grain g = grainParams(true);
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
                t, (double)Pi, std::floor(anchor * 16), recStart * 16, recEnd * 16,
                (recEnd - recStart) * 16, timer * 4096, ppw * 4096, ch[0].age * 4096, ch[1].age * 4096,
                std::floor(ch[0].start * 16), std::floor(ch[1].start * 16), next ^ 1, ch[0].e, ch[1].e,
                ch[0].v, ch[1].v, ch[0].tapW, ch[1].tapW, slope, ch[0].tap * 16, ch[1].tap * 16);

        double y[2] = {};
        for (Chan& c : ch) {
            const double e = std::min(c.e, 1.0);
            const double w = 2.0 * std::min(e, 1.0 - e);
            if (w > 0.0) {
                const double pos = c.start + c.age * c.v;
                y[0] += w * readGrain(p->in, pos, c.tap, c.tapW);
                if (st) y[1] += w * readGrain(p->inR, pos, c.tap, c.tapW);
            }
            c.e = e + slope;
            c.age += 1.0;
        }

        // Highpass SVF, one per channel with shared coefficients (+0x54/+0x58
        // left, +0x5c/+0x60 right)
        double high[2];
        for (int c = 0; c < (st ? 2 : 1); ++c) {
            low[c] += fCoef * band[c];
            high[c] = y[c] - q * band[c] - low[c];
            band[c] += fCoef * high[c];
        }
        // output gain (0x8950d1c2): gain += (target − gain)/64, then the scale
        gain = xtf::add(xtf::mul(xtf::sub(gainTarget, gain), 0.015625f), gain);   // fmac: product, then sum
        out[k] = (float)(high[0] * gain);
        if (right) right[k] = (float)(high[st ? 1 : 0] * gain);
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
