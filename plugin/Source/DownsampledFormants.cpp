// SourceFilterModel's downsampled formant analysis: DownsampledFormants.h.  Moved whole from
// SourceFilterModel::computeDownsampledFormantBiquads in v176, by its steps, with its smoothing state the instance's.
#include "DownsampledFormants.h"
#include "PolynomialRoots.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace VSE {

namespace {

constexpr int kDecim = 4;   // 48 kHz → 12 kHz analysis rate
constexpr int kOrd = DownsampledFormants::kOrd;
using Lags = DownsampledFormants::Lags;
using Section = DownsampledFormants::Section;

// ── 1. Anti-alias FIR (windowed sinc, built once) + decimate ─────────────
// ── 2. Pre-emphasis at the decimated rate ────────────────────────────────
// Standard practice for formant LPC: at 12 kHz the speech band fills the
// full bandwidth, so pre-emphasis flattens harmonic decay without pushing
// poles to a noise floor (there is none — it was filtered out).
std::vector<float> decimate(const std::vector<float>& frame) {
    static const std::vector<float> fir = [] {
        const int    taps = 33;
        const double fc   = 0.45 / double(kDecim);
        std::vector<float> h(taps);
        double sum = 0.0;
        for (int i = 0; i < taps; ++i) {
            const int    m = i - taps / 2;
            const double s = (m == 0) ? 2.0 * fc
                : std::sin(2.0 * std::numbers::pi * fc * m) / (std::numbers::pi * m);
            const double w = 0.54 - 0.46 * std::cos(2.0 * std::numbers::pi * i / (taps - 1));
            h[i] = float(s * w);
            sum += h[i];
        }
        for (auto& v : h) v = float(v / sum);
        return h;
    }();

    const int frameSize = static_cast<int>(frame.size());
    const int N = frameSize / kDecim;
    std::vector<float> dec(N);
    const int half = (int)fir.size() / 2;
    for (int n = 0; n < N; ++n) {
        const int center = n * kDecim;
        float acc = 0.0f;
        for (int t = 0; t < (int)fir.size(); ++t) {
            const int idx = center + t - half;
            if (idx >= 0 && idx < frameSize) acc += fir[t] * frame[idx];
        }
        dec[n] = acc;
    }
    for (int i = N - 1; i > 0; --i) dec[i] -= 0.97f * dec[i - 1];
    return dec;
}

// ── 3. The autocorrelation, to lag kOrd ──────────────────────────────────
Lags autocorrelation(const std::vector<float>& dec) {
    const int N = static_cast<int>(dec.size());
    Lags r {};
    for (int lag = 0; lag <= kOrd; ++lag)
        for (int n2 = lag; n2 < N; ++n2)
            r[lag] += double(dec[n2]) * double(dec[n2 - lag]);
    return r;
}

// ── Levinson-Durbin, order 12, then bandwidth expansion; false if unstable
bool predictor(const Lags& r, Lags& a) {
    double err = r[0];
    for (int i = 1; i <= kOrd; ++i) {
        double acc = r[i];
        for (int j = 1; j < i; ++j) acc -= a[j] * r[i - j];
        const double k = acc / err;
        if (!(std::abs(k) < 1.0)) return false;
        double tmp[kOrd + 1];
        for (int j = 1; j < i; ++j) tmp[j] = a[j] - k * a[i - j];
        for (int j = 1; j < i; ++j) a[j] = tmp[j];
        a[i] = k;
        err *= (1.0 - k * k);
        if (err <= 0.0) return false;
    }
    // Bandwidth expansion (stability margin), λ = 0.994
    double lk = 0.994;
    for (int i = 1; i <= kOrd; ++i) {
        a[i] *= lk;
        lk *= 0.994;
    }
    return true;
}

// ── 4. Roots of z^12 − a₁z^11 − … − a₁₂ (Laguerre + deflation + polish) ──
std::vector<std::complex<double>> poles(const Lags& a) {
    std::vector<std::complex<double>> fullPoly(kOrd + 1);
    fullPoly[0] = 1.0;
    for (int i = 1; i <= kOrd; ++i) fullPoly[i] = -a[i];
    return polynomialRoots(fullPoly);
}

// ── 5. Map poles to the full rate, shift angles, build biquads ───────────
// Frequency is preserved in Hz: θ48 = θ12 / kDecim.  Bandwidth in Hz is
// preserved: r48 = r12^(1/kDecim).  The formant shift scales θ48.
// Real poles (spectral tilt / DC) are DROPPED for this path: keeping
// them unshifted leaves a strong low-frequency resonance that the
// V-Synth's shifted envelope does not have (its up-shift rolls off
// below the new F1).  The per-frame energy normalisation absorbs the
// overall gain change.
void sections(const std::vector<std::complex<double>>& roots, float shiftRatio, std::vector<Section>& out) {
    out.clear();
    for (const auto& rt : roots) {
        const double absR = std::abs(rt);
        if (absR < 1e-12) continue;
        if (std::abs(rt.imag()) > 1e-5 * absR && rt.imag() > 0.0) {
            const double mag48 = std::min(std::pow(std::min(absR, 0.999), 1.0 / kDecim), 0.995);
            double theta = (std::arg(rt) / double(kDecim)) * double(shiftRatio);
            theta = std::min(theta, 3.0);          // keep below π
            out.push_back({ float(-2.0 * mag48 * std::cos(theta)), float(mag48 * mag48) });
        }
    }
}

} // namespace

// The metric's formant-similarity score compares TRAJECTORIES; per-frame
// envelope jitter tanks it even when the time-averaged formants match the
// reference.  Smooth in the autocorrelation domain (well-defined however
// the pole configuration changes between frames).
void DownsampledFormants::smooth(Lags& r) {
    const double e0 = r[0];
    if (smoothed_[0] <= 0.0) {   // the first frame since reset(): its own
        for (int i = 0; i <= kOrd; ++i) smoothed_[i] = r[i] / e0;
    } else {
        constexpr double alpha = 0.7;
        for (int i = 0; i <= kOrd; ++i)
            smoothed_[i] = alpha * smoothed_[i] + (1.0 - alpha) * (r[i] / e0);
    }
    for (int i = 0; i <= kOrd; ++i) r[i] = smoothed_[i] * e0;
}

bool DownsampledFormants::analyse(const std::vector<float>& frame, float shiftRatio, std::vector<Section>& out) {
    Lags r = autocorrelation(decimate(frame));
    if (r[0] < 1e-9) return false;
    smooth(r);
    Lags a {};
    if (!predictor(r, a)) return false;
    sections(poles(a), shiftRatio, out);
    return !out.empty();
}

} // namespace VSE
