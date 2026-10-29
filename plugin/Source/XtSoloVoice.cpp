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
// sets ppw, slope, the anchor step and (voiced only) f0n.  Computed as the
// firmware does, in single precision on the 1/16-sample grid: the truncated
// results (os, ppw, the anchor step) differ by a grid step in double when the
// exact value lands on the grid (os = 5365·(1 − 0.8) = 1073).
XtSoloVoice::Grain XtSoloVoice::grainParams(size_t sg, bool setF0) {
    const size_t nc = p->size();
    // Only the PERIOD is floored (160/16 samples); the record's extent, the
    // anchor wrap and the second-tap offset use the raw length.
    const float C = std::max(160.0f, (float)(16.0 * p->segLen[sg]));
    // After the last record the XT's stream holds a (256, 1) terminator: the
    // last record's next period is 16 samples.
    const float N = std::max(160.0f, (sg + 1 < nc) ? (float)(16.0 * p->segLen[sg + 1]) : 256.0f);
    float frac = 0.0f, Ci = C;
    if (Pq > p->segStart[sg] + p->segLen[sg]) { frac = 1.0f; Ci = N; }
    else if (Pq > p->segStart[sg]) {
        frac = xtf::div((float)(16.0 * (Pq - p->segStart[sg])), C);
        Ci = xtf::add(xtf::mul(frac, xtf::sub(N, C)), C);          // fmac: product, then sum
    }
    bool voiced = p->voiced[sg] != 0;
    if (p->forceUnvoiced) voiced = false;
    if (p->forcedInfo >= 0) voiced = (p->forcedInfo & 4) != 0;
    const int info = p->forcedInfo >= 0 ? p->forcedInfo : p->info[sg];
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
    return { p->segStart[sg] + os / 16.0, v, frac, p->segLen[sg] };
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
    low[0] = low[1] = band[0] = band[1] = fCoef = 0.0;
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

void XtSoloVoice::render(float* out, float* right, int count) {
    if (!p || p->size() < 2) {
        std::fill(out, out + count, 0.0f);
        if (right) std::fill(right, right + count, 0.0f);
        return;
    }
    const bool st = right && p->stereo();
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
        out[k] = (float)high[0];
        if (right) right[k] = (float)high[st ? 1 : 0];
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
    voice.noteOn(plan, R, F, 1.0 / walk, energy);
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
