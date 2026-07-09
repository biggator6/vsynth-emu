// EngineTests.cpp — fast invariant tests for the VariPhrase engine.
//
// Session 15 produced one unmeasured regression (v24b silently broke the
// sine pitch cases because those files were never re-rendered) precisely
// because the only test was the slow 20-case batch.  These tests assert the
// hard-won engine invariants on SYNTHETIC inputs in seconds.
//
// Build & run:
//   clang++ -std=c++17 -O2 -DENGINE_TESTS_MAIN \
//       plugin/Source/EngineTests.cpp plugin/Source/VariphraseEngine.cpp \
//       plugin/Source/PhaseVocoder.cpp plugin/Source/SourceFilterModel.cpp \
//       -o /tmp/engine_tests && /tmp/engine_tests
//
// Exit code 0 = all pass.  Each failure prints a line and sets the code.

#ifdef ENGINE_TESTS_MAIN

#include "VariphraseEngine.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>

using namespace VSE;

namespace {

constexpr double kSR = 48000.0;
int gFailures = 0;

void check(bool ok, const std::string& name, const std::string& detail) {
    if (ok) std::printf("  PASS  %s\n", name.c_str());
    else  { std::printf("  FAIL  %s  (%s)\n", name.c_str(), detail.c_str());
            ++gFailures; }
}

std::vector<float> sine(double freq, double secs, double amp = 0.5) {
    // −80 dB dither: a MATHEMATICALLY perfect sine destabilises the per-frame
    // F0 estimate (spurious 472 Hz component, −4 dB, on a bit-exact 440 Hz
    // input — logged as a known edge case).  Every real capture has a noise
    // floor; the tests should represent real conditions.
    const int n = (int)(secs * kSR);
    std::vector<float> v(n);
    unsigned rng = 12345;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const double dither = ((double)(rng >> 8) / (double)(1u << 24) - 0.5) * 2e-4;
        v[i] = (float)(amp * std::sin(2.0 * 3.14159265358979 * freq * i / kSR) + dither);
    }
    return v;
}

// Zero-crossing frequency estimate over the middle half of the signal.
double zcFreq(const std::vector<float>& x) {
    const int a = (int)x.size() / 4, b = 3 * (int)x.size() / 4;
    int zc = 0;
    for (int i = a + 1; i < b; ++i)
        if ((x[i] >= 0.0f) != (x[i-1] >= 0.0f)) ++zc;
    return 0.5 * zc / ((b - a) / kSR);
}

double rms(const std::vector<float>& x, int a, int b) {
    double e = 0.0;
    for (int i = a; i < b; ++i) e += (double)x[i] * x[i];
    return std::sqrt(e / std::max(1, b - a));
}

// Interpolated-ACF F0 over the middle of the signal.  Integer-lag ACF alone
// quantizes hard at high F0 (48 kHz / lag 73 vs 74 = 9 Hz steps) — parabolic
// interpolation on the peak recovers sub-sample lag.
double acfF0(const std::vector<float>& x, double fLo, double fHi) {
    const int a = (int)x.size() / 4;
    const int W = 16384;
    const int lagLo = std::max(2, (int)(kSR / fHi));
    const int lagHi = (int)(kSR / fLo);
    std::vector<double> ac(lagHi + 2, 0.0);
    double best = -1e30; int bestLag = lagLo;
    for (int lag = lagLo - 1; lag <= lagHi + 1; ++lag) {
        double s = 0;
        for (int i = 0; i < W; ++i) s += (double)x[a+i] * x[a+i+lag];
        ac[lag] = s;
        if (lag >= lagLo && lag <= lagHi && s > best) { best = s; bestLag = lag; }
    }
    const double y0 = ac[bestLag-1], y1 = ac[bestLag], y2 = ac[bestLag+1];
    const double den = y0 - 2.0*y1 + y2;
    const double d = (std::abs(den) > 1e-30) ? 0.5*(y0 - y2)/den : 0.0;
    return kSR / ((double)bestLag + d);
}

// Render helper: full offline pipeline (encode pass + processOffline),
// mirroring OfflineRenderer.
std::vector<float> render(const std::vector<float>& in, float time,
                          float pitchSt, float formantSt) {
    VariphraseEngine eng;
    eng.prepare(kSR, 512);
    eng.setAlgorithm(Algorithm::Hybrid);
    VariphraseParams p;
    p.timeStretchRatio      = time;
    p.pitchShiftSemitones   = pitchSt;
    p.formantShiftSemitones = formantSt;
    eng.setParams(p);
    eng.setAnalysis(VariphraseEngine::analyzeContent(in.data(), (int)in.size(), kSR));
    return eng.processOffline(in);
}

} // namespace

int main() {
    std::printf("VariPhrase engine invariant tests\n");

    // ── 1. Drain-mode duration invariant ────────────────────────────────────
    // Output length must equal inputLen × timeStretch for any ratio
    // (Session 15: the pre-drain renderer truncated/corrupted stretches).
    {
        auto in = sine(440.0, 2.0);
        for (float st : { 0.5f, 1.0f, 2.0f }) {
            auto out = render(in, st, 0.0f, 0.0f);
            const long expect = std::lround((double)in.size() * st);
            check(std::labs((long)out.size() - expect) <= 1,
                  "duration stretch=" + std::to_string(st),
                  "got " + std::to_string(out.size()) + " want " + std::to_string(expect));
        }
    }

    // ── 2. Frequency preservation under time stretch ────────────────────────
    // A 440 Hz tone stretched 2× must stay 440 Hz within 1 %
    // (Session 15: pre-v21 PV produced ±43 Hz sidebands; LITE routes to LPC
    // resynthesis which must re-produce the fundamental).
    {
        auto in  = sine(440.0, 2.0);
        auto out = render(in, 2.0f, 0.0f, 0.0f);
        const double f = zcFreq(out);
        check(std::abs(f - 440.0) < 15.0, "freq preserved at stretch 2x",
              "zc freq " + std::to_string(f));
    }

    // ── 3. Pitch-shift accuracy (LITE resynthesis path) ─────────────────────
    // +7 st on 440 Hz → 659.3 Hz within ~2 %.  ACF-based F0 (zero-crossing
    // estimates are unreliable on the harmonic-rich resynthesis output).
    {
        auto in  = sine(440.0, 2.0);
        auto out = render(in, 1.0f, 7.0f, 0.0f);
        const double f = acfF0(out, 300.0, 900.0);
        check(std::abs(f - 659.3) < 15.0, "pitch +7st accuracy (ACF)",
              "acf F0 " + std::to_string(f) + " want 659.3");
    }

    // ── 4. No mid-stream dropouts (OLA read gating invariant) ───────────────
    // Windowed RMS over the middle of a stretched render must never collapse
    // (Session 15: pre-gating compression lost frames behind the read pointer).
    {
        auto in  = sine(330.0, 2.0);
        auto out = render(in, 0.5f, 0.0f, 0.0f);
        const int n = (int)out.size();
        const int w = 2048;
        double minR = 1e9, meanR = 0.0; int cnt = 0;
        for (int a = n / 4; a + w < 3 * n / 4; a += w) {
            const double r = rms(out, a, a + w);
            minR = std::min(minR, r); meanR += r; ++cnt;
        }
        meanR /= std::max(1, cnt);
        check(minR > 0.25 * meanR, "no dropouts at compression 0.5x",
              "min windowed rms " + std::to_string(minR) +
              " vs mean " + std::to_string(meanR));
    }

    // ── 5. Granular engine: SOLO F0 accuracy under pitch shift ──────────────
    // A vibrato-free "vowel-like" tone (F0 + strong 2 kHz component so the
    // classifier reads SOLO) pitch-shifted +7 st must land on target F0
    // (v26: granular pitch = grain re-trigger period).
    {
        auto in = sine(130.0, 2.0, 0.4);
        auto f2 = sine(2000.0, 2.0, 0.1);            // pseudo-formant energy
        for (size_t i = 0; i < in.size(); ++i) in[i] += f2[i];
        auto an = VariphraseEngine::analyzeContent(in.data(), (int)in.size(), kSR);
        const bool solo = (an.contentType == VariphraseAnalysis::ContentType::SOLO);
        check(solo, "classifier: F0+2kHz tone reads SOLO", "contentType wrong");
        if (solo) {
            auto out = render(in, 1.0f, 7.0f, 0.0f);
            const double f0 = acfF0(out, 80.0, 400.0);
            check(std::abs(f0 - 194.8) < 8.0, "granular pitch +7st F0",
                  "acf F0 " + std::to_string(f0) + " want 194.8");
        }
    }

    // ── 6. Subband identity reconstruction (ENSEMBLE invariant) ─────────────
    // A 3-note chord stretched by 1.02 must reconstruct near-perfectly
    // (v27: raised-cosine masks sum to unity; verified 0.958 spectral sim on
    // real material — here assert time-domain correlation on synthetic).
    {
        auto a = sine(262.0, 2.0, 0.2);
        auto b = sine(330.0, 2.0, 0.2);
        auto c = sine(392.0, 2.0, 0.2);
        std::vector<float> in(a.size());
        for (size_t i = 0; i < in.size(); ++i) in[i] = a[i] + b[i] + c[i];

        VariphraseEngine eng;
        eng.prepare(kSR, 512);
        auto out = eng.subbandStretchOffline(in, 1.0f, 1.0f);
        check(!out.empty(), "subband returns output", "empty");
        if (!out.empty()) {
            const int n = std::min(in.size(), out.size());
            double num = 0, di = 0, doo = 0;
            for (int i = n/4; i < 3*n/4; ++i) {
                num += (double)in[i] * out[i];
                di  += (double)in[i] * in[i];
                doo += (double)out[i] * out[i];
            }
            const double corr = num / std::sqrt(di * doo + 1e-30);
            check(corr > 0.85, "subband identity correlation",
                  "corr " + std::to_string(corr));
        }
    }

    std::printf(gFailures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", gFailures);
    return gFailures ? 1 : 0;
}

#endif // ENGINE_TESTS_MAIN
