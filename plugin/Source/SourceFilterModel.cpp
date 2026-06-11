#include "SourceFilterModel.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <complex>

namespace VSE {

static constexpr float kTwoPi = 6.28318530718f;

// ─── FFT helper for cepstral LPC (Cooley-Tukey iterative, in-place) ──────────
// Same bit-reversal + butterfly convention as PhaseVocoder::fft().
//   inverse=false : forward DFT  (twiddle = exp(+j·2π/len))
//   inverse=true  : IDFT         (twiddle = exp(−j·2π/len), result ÷ N)
// N must be a power of two.  Called exclusively from computeLPCCepstral.

static void sfmFft(std::vector<std::complex<float>>& data, bool inverse)
{
    const int N = static_cast<int>(data.size());
    if (N <= 1) return;
    // Bit-reversal permutation
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }
    // DIT butterfly
    for (int len = 2; len <= N; len <<= 1) {
        const float ang = kTwoPi / float(len) * (inverse ? -1.0f : 1.0f);
        const std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            const int half = len >> 1;
            for (int j = 0; j < half; ++j) {
                const auto u = data[i + j];
                const auto v = data[i + j + half] * w;
                data[i + j]        = u + v;
                data[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse)
        for (auto& x : data) x /= float(N);
}

// ─── Construction ─────────────────────────────────────────────────────────────

SourceFilterModel::SourceFilterModel() {
    window_.resize(kFrameSize);
    for (int i = 0; i < kFrameSize; ++i)
        window_[i] = 0.5f * (1.0f - std::cos(kTwoPi * float(i) / float(kFrameSize - 1)));
}

SourceFilterModel::~SourceFilterModel() = default;

void SourceFilterModel::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;

    // Autocorrelation of the SQUARED window, up to the largest F0 search lag
    // (60 Hz minimum).  estimateF0 sees the frame tapered by Hann² (windowed
    // once at extraction, once inside estimateF0), so the taper-bias
    // correction must use w² as the effective window.
    {
        const int maxLag = static_cast<int>(sampleRate_ / 60.0);
        winAcf_.assign(maxLag + 1, 0.0f);
        for (int lag = 0; lag <= maxLag && lag < kFrameSize; ++lag) {
            float s = 0.0f;
            for (int i = 0; i + lag < kFrameSize; ++i)
                s += (window_[i] * window_[i]) * (window_[i + lag] * window_[i + lag]);
            winAcf_[lag] = s;
        }
    }

    inputBuffer_.assign(kFrameSize * 4, 0.0f);
    outputBuffer_.assign(kFrameSize * 16, 0.0f);
    outputBufferPlain_.assign(kFrameSize * 16, 0.0f);
    lpcCoeffs_.assign(kLPCOrder, 0.0f);
    filterState_.assign(kLPCOrder, 0.0f);
    biquadState_.assign(kLPCOrder / 2, {0.0f, 0.0f});
    sawPhase_ = 0.0f;

    reset();
}

void SourceFilterModel::setOutputCapacity(int samples) {
    if (samples > (int)outputBuffer_.size()) {
        outputBuffer_.assign(samples, 0.0f);
        outputBufferPlain_.assign(samples, 0.0f);
    }
}

void SourceFilterModel::reset() {
    std::fill(inputBuffer_.begin(),  inputBuffer_.end(),  0.0f);
    std::fill(outputBuffer_.begin(), outputBuffer_.end(), 0.0f);
    std::fill(outputBufferPlain_.begin(), outputBufferPlain_.end(), 0.0f);
    std::fill(filterState_.begin(),  filterState_.end(),  0.0f);
    for (auto& s : biquadState_) { s[0] = 0.0f; s[1] = 0.0f; }
    biquads_.clear();
    useBiquad_      = false;
    prevFrameRMS_   = 0.0f;
    inputWritePos_  = 0;
    inputReadPos_   = 0;
    inputFill_      = 0;
    outputWritePos_ = 0;
    outputReadPos_  = 0;
    outputAvail_    = 0;
    lastValidOutput_= 0;
    synthHopAccum_  = 0.0f;
    sawPhase_       = 0.0f;
    lpcGain_        = 1.0f;
    deEmphState_    = 0.0f;
}

void SourceFilterModel::setParams(const VariphraseParams& params) {
    params_ = params;
}

int SourceFilterModel::getLatencySamples() const {
    return kFrameSize;
}

// ─── LPC Analysis (Levinson-Durbin) ──────────────────────────────────────────

void SourceFilterModel::computeLPC(const std::vector<float>& frame,
                                    std::vector<float>& coeffs,
                                    float& gain,
                                    int minGuardOrder) {
    const int N = static_cast<int>(frame.size());
    coeffs.assign(kLPCOrder, 0.0f);
    gain = 0.0f;

    // Autocorrelation
    std::vector<double> r(kLPCOrder + 1, 0.0);
    for (int lag = 0; lag <= kLPCOrder; ++lag)
        for (int n = lag; n < N; ++n)
            r[lag] += double(frame[n]) * double(frame[n - lag]);

    if (r[0] < 1e-10) return;

    // Levinson-Durbin with adaptive order
    //
    // Stability invariants:
    //   1. error > 0 must hold before dividing.  Pure sinusoids yield near-zero
    //      prediction error at order 2, so subsequent orders must be truncated.
    //   2. |lam| < 1 must hold (reflection coefficient).  |lam| ≥ 1 means the
    //      analysis has hit a degenerate frame; higher orders are meaningless
    //      and would produce a catastrophically unstable synthesis filter
    //      (confirmed: causes NaN after ~512 output samples).
    //   3. Adaptive early stop: if the normalised prediction error
    //      (error / r[0]) falls below kEarlyStopThreshold, the model already
    //      explains the signal well — further orders fit residual noise or
    //      numerical artefacts rather than genuine spectral structure.
    //
    // Why Guard 3 matters:
    //   A pure 440 Hz Hann-windowed sinusoid is almost perfectly predicted by
    //   a 2nd-order AR model (two poles at ±440 Hz).  The reflection coefficient
    //   at order 2 is |lam₂| ≈ 0.999975, which is below the |lam|≥1 guard, so
    //   without Guard 3 Levinson-Durbin runs all 16 orders, producing 16 poles
    //   clustered near 440 Hz.  After a large formant downshift (e.g. −12 st,
    //   ratio 0.5) those 16 poles cluster near 220 Hz, giving a combined filter
    //   gain of ~28 000 at 220 Hz and a formant similarity of only 0.638.
    //
    //   With Guard 3 (threshold = 1%), the loop exits at order 2 for the pure
    //   sine: error after order 2 is r[0]×(1−0.999975²) ≈ 5×10⁻⁵×r[0] ≪ 0.01.
    //   The resulting 2-pole model shifts cleanly to 220 Hz with no clustering.
    //
    // Threshold choice: 0.01 (stop when >99% of variance is explained).
    //   Catches pure/near-pure sinusoids (residual < 0.01%) without stopping
    //   early on complex spectra.
    //
    // NOTE (Session 8 diagnostic): For voiced speech with a strong fundamental
    // (e.g. vowel "aah" at ~120 Hz) the order-2 residual is typically ≈0.14%
    // of r[0], which IS below the 1% threshold — so Guard 3 fires at order 2
    // for speech too.  This causes vocal_aah_formant_downmax to score only
    // 16.4 (formant_sim=0.325) because the 2-pole model captures pitch, not
    // F1–F4.  We attempted removing Guard 3 entirely in Session 8, but this
    // introduced near-Nyquist pole aliasing in the 16-pole model after a large
    // upshift (ratio=2), degrading vocal_aah_formant_upmax from 34.5→30.8.
    //
    // Session 9 fix: voiced-adaptive minimum guard order.
    //   For voiced speech frames, the caller passes minGuardOrder=8.  This
    //   ensures the recursion runs at least 4 conjugate-pair iterations (≈ F1–F4)
    //   before Guard 3 is allowed to stop it.
    //
    // Session 9 (incorrect) diagnosis of pre-emphasis: the Session 9 comment
    //   claimed pre-emphasis gives no benefit at 48 kHz.  The argument was that
    //   r[1]/r[0] stays near 1 regardless, so Guard 3 still fires at order 2.
    //   This was wrong in an important way: Guard 3 firing behaviour is already
    //   prevented by minGuardOrder=8.  The more important effect of pre-emphasis
    //   is on the OPTIMISATION CRITERION used by Levinson-Durbin for orders 2–8:
    //   without PE, the fundamental (k=1) energy dominates by ~10 000× at 48 kHz,
    //   so ALL 8 poles cluster near F0 (~120 Hz).  With PE, |H(kω₀)|²≈0.97(kω₀)²
    //   exactly cancels the 1/k² harmonic decay, making every harmonic contribute
    //   equally — and the 8 poles then spread across F1–F4 as intended.
    //   (Session 13: pre-emphasis applied to the analysis frame before calling
    //   computeLPC; de-emphasis 1/(1−0.97z⁻¹) applied to the OLA output stream.)
    constexpr double kEarlyStopThreshold = 0.01;

    std::vector<double> a(kLPCOrder + 1, 0.0);
    double error = r[0];

    for (int i = 1; i <= kLPCOrder; ++i) {
        // Guard 1: stop before division-by-near-zero
        if (error < 1e-10) break;

        double lam = r[i];
        for (int j = 1; j < i; ++j)
            lam -= a[j] * r[i - j];
        lam /= error;

        // Guard 2: reflection coefficient must be strictly inside the unit disc
        if (std::abs(lam) >= 1.0) break;

        // Coefficient update uses old values — take a snapshot to avoid
        // read-after-write aliasing in the in-place update.
        std::vector<double> old_a(a.begin(), a.begin() + i + 1);
        a[i] = lam;
        for (int j = 1; j < i; ++j)
            a[j] = old_a[j] - lam * old_a[i - j];

        error *= (1.0 - lam * lam);

        // Guard 3: early stop — model explains >99% of signal variance.
        // Keep the current order's coefficients (already written above);
        // do not add more poles.
        //
        // The guard uses minGuardOrder (caller-supplied, default 2):
        //   minGuardOrder=2 → pure sines / unvoiced (one conjugate pair minimum)
        //   minGuardOrder=8 → voiced speech (4 conjugate pairs = F1–F4 minimum)
        //
        // For pure tones: a pure sinusoid needs a conjugate pair (2 poles).
        //   At order 1, error₁ ≈ 0.004×r[0] — already below the 1% threshold.
        //   Breaking at order 1 leaves a single real pole, useless for formant
        //   shift and explosively amplified by energy normalization.
        //
        // For voiced speech: the 2-pole model at order 2 explains ≈99.86% of
        //   variance (fundamental dominates short-lag autocorrelation at 48 kHz).
        //   Guard 3 would fire and the model captures pitch, not formants.
        //   Raising the guard to order 8 forces the recursion to at least 4
        //   conjugate pairs, reliably capturing F1–F4 before stopping.
        if (i >= minGuardOrder && error < kEarlyStopThreshold * r[0]) break;
    }

    for (int i = 0; i < kLPCOrder; ++i)
        coeffs[i] = float(a[i + 1]);

    gain = float(std::sqrt(std::max(error, 1e-10)));
}

// ─── Cepstral-Liftering LPC ───────────────────────────────────────────────────
//
// Motivation:
//   Standard LPC on voiced speech at 48 kHz fails to capture formants because
//   all speech harmonics (F0=120 Hz up to F4=3.5 kHz) have normalised angular
//   frequency ω < 0.46 rad/sample.  The short-lag autocorrelation
//     r[k] = Σ A_h² cos(kω_h) / 2  (sum over harmonics h)
//   therefore has r[k]/r[0] ≈ 1 for k=1..16 regardless of the formant shaping
//   of the harmonic amplitudes A_h.  Guard 3 fires at LPC order 2 because the
//   two-pole pitch model already explains >99% of variance.
//
// Fix — derive autocorrelation from the cepstrally-liftered log spectrum:
//   1. FFT the windowed frame → log power spectrum logP[k]
//   2. IFFT(logP) → cepstrum c[n]  (real-symmetric for real input)
//   3. Lifter: zero cepstral bins L+1..N-L-1
//      L = min(60, T0/4)  where T0 = sampleRate/F0
//      This removes the pitch-harmonic fine structure (cepstral peak at T0)
//      while retaining the smooth spectral envelope (formants, at low quefrency)
//   4. FFT(c_liftered) → smooth log spectrum logP_smooth[k]
//   5. P_smooth[k] = exp(logP_smooth[k])   (smooth power spectrum)
//   6. IFFT(P_smooth) → autocorrelation r[k]   (Wiener-Khinchin)
//   7. Levinson-Durbin on r[0..kLPCOrder] → LPC coefficients
//
//   The autocorrelation derived from the smooth power spectrum is no longer
//   dominated by the fundamental; r[k]/r[0] now reflects the formant bandwidth
//   and frequency, and Levinson-Durbin converges to formant poles rather than
//   pitch poles.
//
// Lifter length L:
//   L = min(60, T0/4) satisfies L < T0/2 (well below the pitch cepstral peak)
//   while being large enough to retain formant structure with bandwidths ≥ 240 Hz.
//   Typical speech formant bandwidths are 100–300 Hz; narrower formants (F1 of
//   some vowels, ~50 Hz) lose some cepstral energy at L=60, but the LPC fit
//   still captures the formant frequency accurately enough for the shift.
//
// Guard 3 (adaptive early stop) is NOT applied inside this function.  The smooth
// spectrum is featureless enough that Levinson-Durbin converges to full order 16
// without the pitch-harmonic bias that caused premature stopping.  Guards 1 & 2
// (numerical stability: near-zero error, |lam|≥1) are still present.
//
// References:
//   Noll (1967), "Cepstrum Pitch Determination", JASA 41(2)
//   Tohkura et al. (1978), "Spectral Smoothing Technique in PARCOR Speech Analysis"

void SourceFilterModel::computeLPCCepstral(const std::vector<float>& frame,
                                            float f0_hz,
                                            std::vector<float>& coeffs,
                                            float& gain)
{
    const int N = kFrameSize;  // 1024 — power of 2, fixed for all frames

    // ── Step 1: Forward FFT of windowed frame ──────────────────────────────────
    std::vector<std::complex<float>> X(N, {0.0f, 0.0f});
    const int fLen = std::min(static_cast<int>(frame.size()), N);
    for (int i = 0; i < fLen; ++i) X[i] = frame[i];
    sfmFft(X, /*inverse=*/false);

    // ── Step 2: Log power spectrum (packed into complex array for IFFT) ────────
    constexpr float kLogFloor = 1e-6f;   // −60 dBFS floor, avoids log(0)
    std::vector<std::complex<float>> C(N);
    for (int i = 0; i < N; ++i)
        C[i] = 0.5f * std::log(std::max(std::norm(X[i]), kLogFloor));
    // C[i] is real here; IFFT will keep the imaginary part near zero

    // ── Step 3: IFFT → cepstrum ────────────────────────────────────────────────
    sfmFft(C, /*inverse=*/true);

    // ── Step 4: Cepstral liftering ─────────────────────────────────────────────
    // Keep bins 0..L and N-L..N-1 (symmetric around 0); zero the rest.
    // L is bounded by T0/4 (well below the pitch-period cepstral peak at T0)
    // and capped at 60 (retains formants with bandwidth ≥ 240 Hz at 48 kHz).
    const int T0 = std::max(2, std::min(static_cast<int>(sampleRate_ / f0_hz + 0.5f),
                                         N / 2));
    const int L  = std::min(60, T0 / 4);

    for (int i = L + 1; i < N - L; ++i) C[i] = 0.0f;

    // ── Step 5: FFT of liftered cepstrum → smooth log spectrum ────────────────
    sfmFft(C, /*inverse=*/false);

    // ── Step 6: Smooth power spectrum (exp of real part) ──────────────────────
    // C[i].real() is the smooth log spectrum; imaginary part is near-zero
    // numerical noise from the real-symmetric cepstrum.
    std::vector<std::complex<float>> P(N);
    for (int i = 0; i < N; ++i)
        P[i] = std::exp(C[i].real());

    // ── Step 7: IFFT of smooth power spectrum → autocorrelation ───────────────
    sfmFft(P, /*inverse=*/true);
    // P[k].real() is now the autocorrelation r[k].  The imaginary parts are
    // near-zero (real-even power spectrum → real-even autocorrelation).

    // ── Step 8: Extract r[0..kLPCOrder] ───────────────────────────────────────
    std::vector<double> r(kLPCOrder + 1);
    for (int k = 0; k <= kLPCOrder; ++k)
        r[k] = static_cast<double>(P[k].real());

    if (r[0] < 1e-10) {
        coeffs.assign(kLPCOrder, 0.0f);
        gain = 0.0f;
        return;
    }

    // ── Step 9: Levinson-Durbin on smooth autocorrelation ─────────────────────
    // Guard 3 (early-stop) is intentionally omitted here: the smooth spectrum
    // does not exhibit the pitch-harmonic autocorrelation bias (r[k]/r[0] ≈ 1)
    // that caused premature stops in computeLPC.  Guards 1 & 2 remain for
    // numerical safety (near-zero error, |lam| ≥ 1).
    std::vector<double> a(kLPCOrder + 1, 0.0);
    double error = r[0];

    for (int i = 1; i <= kLPCOrder; ++i) {
        if (error < 1e-10) break;               // Guard 1: near-zero denominator

        double lam = r[i];
        for (int j = 1; j < i; ++j) lam -= a[j] * r[i - j];
        lam /= error;

        if (std::abs(lam) >= 1.0) break;         // Guard 2: unstable reflection coeff

        std::vector<double> old_a(a.begin(), a.begin() + i + 1);
        a[i] = lam;
        for (int j = 1; j < i; ++j)
            a[j] = old_a[j] - lam * old_a[i - j];

        error *= (1.0 - lam * lam);
        // No Guard 3 here — smooth spectrum is well-conditioned to full order 16
    }

    coeffs.assign(kLPCOrder, 0.0f);
    for (int i = 0; i < kLPCOrder; ++i)
        coeffs[i] = static_cast<float>(a[i + 1]);
    gain = static_cast<float>(std::sqrt(std::max(error, 1e-10)));
}

// ─── Formant Shift via Root (Pole) Angle Scaling ─────────────────────────────
//
// Each conjugate pole pair r = ρ·e^{jθ} has a formant at f = θ·sr/(2π).
// Scaling θ by 'ratio' shifts the formant frequency by that ratio.
// Bandwidth ρ is preserved (bandwidth-invariant shift).
//
// Root-finder: Laguerre's method with PAIRED conjugate deflation.
//
// Key design decision: all arithmetic stays REAL throughout.
//
// The previous approach deflated by single complex roots, causing the remaining
// polynomial to accumulate complex coefficients.  Subsequent Laguerre calls then
// converged to already-found roots or numerically spurious roots, and the final
// Vieta reconstruction (complex polynomial multiplication) suffered catastrophic
// cancellation for clustered poles (e.g. 16 poles near 440 Hz).  The result was
// effective poles at |z|≈2.4 → exponential blowup → NaN in the synthesis filter.
//
// Fix: always extract conjugate pairs (z−r)(z−r*) = z²−2Re(r)z+|r|² together.
// This 2nd-order factor has exact real coefficients computed from (|r|, arg(r)).
// Deflation and reconstruction both stay in double-precision real arithmetic,
// avoiding all imaginary cancellation.
//
// References:
//   Press et al., "Numerical Recipes in C++", 3rd ed., §9.5 (Laguerre's Method)
//   Jenkins & Traub (1970), "A Three-Stage Algorithm for Real Polynomials"

void SourceFilterModel::shiftFormants(std::vector<float>& coeffs, float semitones) {
    if (std::abs(semitones) < 0.01f) return;

    const double ratio = std::pow(2.0, double(semitones) / 12.0);

    // ── Effective LPC order ───────────────────────────────────────────────────
    // When the adaptive early-stop in computeLPC exits before kLPCOrder, the
    // remaining coefficients are zero.  Building a 16th-degree polynomial from a
    // 2nd-order model would produce 14 spurious roots at z=0 that the root-finder
    // must chase, and the factor count check would reject the result.
    //
    // Instead, find the index of the last non-zero coefficient and use that as the
    // effective polynomial degree.  For a pure sinusoid stopped at order 2, this
    // gives n=2, one conjugate pair, and 1 biquad section — the correct result.
    int n = 0;
    for (int i = kLPCOrder - 1; i >= 0; --i) {
        if (std::abs(coeffs[i]) > 1e-8f) { n = i + 1; break; }
    }
    if (n < 2) return;   // too few poles — nothing useful to shift
    if (n % 2 != 0) ++n; // round up to even (one extra zero coeff = one pole at origin)
    n = std::min(n, kLPCOrder);

    // Build Q(z) = z^n − a₁z^{n−1} − … − aₙ whose roots are the synthesis filter poles.
    // Q[0]=1, Q[k]=−aₖ (negated LPC coefficient array, up to effective order n).
    std::vector<std::complex<double>> fullPoly(n + 1);
    fullPoly[0] = 1.0;
    for (int i = 0; i < n; ++i)
        fullPoly[i + 1] = -double(coeffs[i]);

    // ── Laguerre step (complex polynomial, complex iterate) ──────────────────
    auto laguerreStep = [](const std::vector<std::complex<double>>& p, int deg,
                           std::complex<double> x) -> std::complex<double> {
        std::complex<double> pv = p[0], dp = 0.0, d2p = 0.0;
        for (int k = 1; k <= deg; ++k) {
            d2p = d2p * x + dp;
            dp  = dp  * x + pv;
            pv  = pv  * x + p[k];
        }
        d2p *= 2.0;
        if (std::abs(pv) < 1e-30) return x;
        const std::complex<double> G   = dp / pv;
        const std::complex<double> H   = G * G - d2p / pv;
        const std::complex<double> sq  = std::sqrt(std::complex<double>(deg - 1) *
                                                    (std::complex<double>(deg) * H - G * G));
        const std::complex<double> d1  = G + sq;
        const std::complex<double> d2  = G - sq;
        const std::complex<double> den = (std::abs(d1) >= std::abs(d2)) ? d1 : d2;
        if (std::abs(den) < 1e-30) return x;
        return x - std::complex<double>(deg) / den;
    };

    // ── Root finding: Laguerre + single-root deflation + polishing ───────────
    // Single-root deflation accumulates error, but polishing on the full
    // polynomial corrects it.  We do NOT use the deflated polynomial for the
    // final root values — only for steering subsequent Laguerre calls.
    std::vector<std::complex<double>> roots;
    roots.reserve(n);
    {
        std::vector<std::complex<double>> deflated = fullPoly;
        for (int i = n; i >= 1; --i) {
            std::complex<double> x(0.2 + 0.07 * double(i), 0.3 - 0.05 * double(i));
            for (int iter = 0; iter < 200; ++iter) {
                auto xnew = laguerreStep(deflated, i, x);
                double ch = std::abs(xnew - x); x = xnew;
                if (ch < 1e-12 * (1.0 + std::abs(x))) break;
            }
            // Polish on full polynomial to remove deflation error
            for (int iter = 0; iter < 60; ++iter) {
                auto xnew = laguerreStep(fullPoly, n, x);
                double ch = std::abs(xnew - x); x = xnew;
                if (ch < 1e-14 * (1.0 + std::abs(x))) break;
            }
            roots.push_back(x);
            // Single-root Horner deflation
            std::vector<std::complex<double>> q(i);
            q[0] = deflated[0];
            for (int k = 1; k < i; ++k)
                q[k] = deflated[k] + x * q[k - 1];
            deflated = q;
        }
    }

    // ── Classify roots and build shifted 2nd-order factors ───────────────────
    //
    // The LPC polynomial has real coefficients → roots come in conjugate pairs
    // (α, ᾱ) for complex roots, or are real (α = ᾱ).
    //
    // After angle-scaling by ratio, each conjugate pair stays a conjugate pair,
    // giving a real-coefficient 2nd-order factor.  Real roots stay real (DC,
    // angle=0) or become a complex conjugate pair (Nyquist, angle=π → π·ratio).
    //
    // Classification strategy:
    //   Im(r) >  ε : upper-half-plane complex root → forms one conjugate pair
    //                factor with its (implicit) lower-half conjugate
    //   |Im(r)| ≤ ε : real root → collect, pair arbitrarily, no formant shift
    //   Im(r) < -ε : lower-half-plane complex root → skip (handled by conjugate)
    //
    // Real roots represent DC / Nyquist content, not vocal-tract formants;
    // leaving them unshifted is the standard practice in LPC-based vocoders.
    // Shifting negative real roots (angle π) by ratio < 1 would move them into
    // the complex plane, requiring pairing with roots not present in the original
    // polynomial — numerically undefined for the LPC angle-scaling operation.

    struct QuadFactor { double b1, b0; };   // z² + b1·z + b0
    std::vector<QuadFactor> factors;
    factors.reserve(n / 2);

    std::vector<double> realRoots;

    for (const auto& r : roots) {
        const double absIm = std::abs(r.imag());
        const double absR  = std::abs(r);
        if (absR < 1e-12) continue;   // root at origin → skip (will be added as real)

        if (absIm > 1e-5 * absR) {
            // Complex root
            if (r.imag() > 0.0) {
                // Upper half-plane: represents one conjugate pair
                double mag   = std::min(absR, 0.995);
                double theta = std::arg(r) * ratio;   // shift formant angle
                factors.push_back({ -2.0 * mag * std::cos(theta), mag * mag });
            }
            // Lower half-plane: implicit conjugate, handled above — skip
        } else {
            // Real root (DC or Nyquist): collect, do not shift
            double clamped = std::min(absR, 0.995) * (r.real() >= 0.0 ? 1.0 : -1.0);
            realRoots.push_back(clamped);
        }
    }

    // Pair real roots into 2nd-order factors with real coefficients.
    // (z − r₁)(z − r₂) = z² − (r₁+r₂)z + r₁r₂
    for (int i = 0; i + 1 < static_cast<int>(realRoots.size()); i += 2)
        factors.push_back({ -(realRoots[i] + realRoots[i + 1]),
                              realRoots[i] * realRoots[i + 1] });
    // Odd real root (shouldn't occur for even-degree real polynomial):
    if (realRoots.size() % 2 != 0)
        factors.push_back({ -realRoots.back(), 0.0 });

    // Safety: if root classification produced wrong factor count, bail out.
    // (Leaves biquads_ empty so processMono falls back to direct-form with the
    // bandwidth-expanded but unshifted coefficients — safe and stable.)
    if (static_cast<int>(factors.size()) != n / 2) return;

    // ── Store shifted factors as cascade biquad sections ──────────────────────
    //
    // Each factor  (z² + b1·z + b0)  implements one 2nd-order all-pole section
    // with transfer function  1 / (1 + b1·z⁻¹ + b0·z⁻²).
    //
    // KEY INSIGHT: we deliberately do NOT reconstruct the combined n-th degree
    // polynomial here.  The combined polynomial for a 16-pole filter tuned to a
    // windowed sinusoid has coefficients that reach ±214 (computed in the session
    // research log).  Casting those to float32 introduces ≈ε_machine·214 ≈ 2.6e-5
    // quantization error per coefficient, which is enough to push poles from
    // |z|=0.994 (stable) to |z|=1.102 (unstable) — verified by np.roots on
    // float32 vs float64 coefficient vectors.
    //
    // In contrast, each biquad's b1 ∈ [−2·|pole|, +2·|pole|] (≤ 2·0.995 = 1.99)
    // and b0 = |pole|² ≤ 0.99.  Float32 represents these exactly to 7 decimal
    // digits — far better than the 2.6e-5 error in the combined polynomial.  The
    // cascade stays analytically stable after the float32 cast.
    biquads_.resize(factors.size());
    for (int i = 0; i < static_cast<int>(factors.size()); ++i) {
        biquads_[i].b1 = float(factors[i].b1);
        biquads_[i].b0 = float(factors[i].b0);
    }
    // coeffs is intentionally left unchanged; it will be ignored by processMono
    // when biquads_ is non-empty.
}

// ─── F0 Detection (Autocorrelation with parabolic interpolation) ──────────────

float SourceFilterModel::estimateF0(const std::vector<float>& frame) const {
    const int N = static_cast<int>(frame.size());

    // Windowed frame for ACF.  The caller's frame is already Hann-windowed;
    // windowing again gives a Hann² taper, whose bias is corrected exactly by
    // winAcf_ (computed on w²) below.  Removing the second window destabilises
    // the estimate (end effects), so keep it.
    //
    // NO pre-emphasis here (removed Session 15): pre-emphasis boosts formant
    // energy, and on real speech the F1-region ACF peaks (~600 Hz) then beat
    // the fundamental — per-frame F0 flapped between ~130 Hz and ~600 Hz,
    // putting the pitch-shifted excitation a fourth sharp.  ACF pitch
    // detection wants the fundamental DOMINANT, the opposite of LPC analysis.
    std::vector<float> x(N);
    for (int i = 0; i < N; ++i)
        x[i] = frame[i] * window_[i < static_cast<int>(window_.size()) ? i : 0];

    // Normalised autocorrelation
    const int minLag = static_cast<int>(sampleRate_ / 800.0);  // max ~800 Hz
    const int maxLag = static_cast<int>(sampleRate_ / 60.0);   // min ~60 Hz

    std::vector<float> acf(maxLag + 1, 0.0f);
    float r0 = 0.0f;
    for (int i = 0; i < N; ++i) r0 += x[i] * x[i];
    if (r0 < 1e-8f) return 0.0f;

    // Unbias by the window autocorrelation: the Hann taper makes the raw ACF
    // decay with lag, which pulls the interpolated peak toward shorter lags
    // (≈1 % sharp at 440 Hz — audible against the coherent OLA).  For a
    // stationary signal, ACF[lag]/winAcf[lag] removes the taper exactly.
    // The correction factor winAcf[0]/winAcf[lag] grows without bound as the
    // window ACF decays toward the frame length, amplifying noise into
    // spurious long-lag peaks.  Clamp it: the F0 lags that matter (≥ 100 Hz →
    // lag ≤ 0.45 N) need factors below ~2; longer lags get a partial
    // correction, which only under-corrects very low F0 estimates slightly.
    const bool haveWinAcf = ((int)winAcf_.size() > maxLag) && (winAcf_[0] > 0.0f);
    for (int lag = minLag; lag <= maxLag; ++lag) {
        float sum = 0.0f;
        for (int i = 0; i < N - lag; ++i)
            sum += x[i] * x[i + lag];
        float corr = 1.0f;
        if (haveWinAcf && winAcf_[lag] > 1e-6f)
            corr = std::min(2.0f, winAcf_[0] / winAcf_[lag]);
        acf[lag] = (sum * corr) / r0;
    }

    // Find best peak — with octave guard.
    //
    // For periodic signals the unbiased ACF has near-equal peaks at the true
    // period and its multiples.  The taper correction above slightly favours
    // LONGER lags (larger correction factor), which without a guard flips the
    // estimate down an octave (sine 440 → 220: the lag-200 peak edged out
    // lag-100 and put a 77 dB subharmonic in the output).  The biased ACF used
    // before Session 15 favoured short lags by construction — an accidental
    // octave guard this code now provides explicitly: take the SHORTEST local
    // maximum within 5 % of the global maximum.
    int   maxIdx = minLag;
    float maxVal = acf[minLag];
    for (int lag = minLag; lag <= maxLag; ++lag)
        if (acf[lag] > maxVal) { maxVal = acf[lag]; maxIdx = lag; }

    if (maxVal < 0.3f) return 0.0f;   // unvoiced threshold

    // Only consider true subharmonic candidates (maxIdx/2, maxIdx/3, …); an
    // earlier "shortest local max within 5 %" rule mistook formant-induced ACF
    // sub-peaks at non-integer fractions of the period for the fundamental
    // (vocal 130.8 Hz was read as 155.5 Hz, putting the +7 st output a fourth
    // sharp).  A genuine octave error puts a near-equal peak at an integer
    // division of the winning lag — search ±2 samples around those only.
    int bestLag = maxIdx;
    for (int div = 4; div >= 2; --div) {
        const int cand = maxIdx / div;
        if (cand < minLag + 1) continue;
        for (int lag = std::max(minLag + 1, cand - 2);
             lag <= std::min(maxLag - 1, cand + 2); ++lag) {
            if (acf[lag] >= 0.95f * maxVal &&
                acf[lag] >= acf[lag - 1] && acf[lag] >= acf[lag + 1]) {
                bestLag = lag;
                div = 1;   // break outer loop — shortest valid divisor wins
                break;
            }
        }
    }

    // Parabolic interpolation for sub-sample accuracy
    if (bestLag > minLag && bestLag < maxLag) {
        float y0 = acf[bestLag - 1], y1 = acf[bestLag], y2 = acf[bestLag + 1];
        float delta = 0.5f * (y0 - y2) / (y0 - 2.0f * y1 + y2 + 1e-10f);
        return float(sampleRate_) / (float(bestLag) + delta);
    }
    return float(sampleRate_) / float(bestLag);
}

bool SourceFilterModel::isVoiced(const std::vector<float>& frame) const {
    const int N = static_cast<int>(frame.size());
    // ZCR heuristic: voiced speech has low zero-crossing rate
    int crossings = 0;
    for (int i = 1; i < N; ++i)
        if ((frame[i] >= 0.0f) != (frame[i-1] >= 0.0f))
            ++crossings;
    float zcr = float(crossings) / float(N);
    // Energy
    float energy = 0.0f;
    for (float s : frame) energy += s * s;
    energy /= float(N);
    return (zcr < 0.15f) && (energy > 1e-6f);
}

// ─── Excitation Synthesis ─────────────────────────────────────────────────────
//
// Synthesises a band-limited sawtooth at f0_hz (num harmonics limited by Nyquist).
// This matches the V-Synth's observed behaviour: a harmonic excitation at the
// detected F0 is used regardless of how harmonic-rich the input was.

void SourceFilterModel::synthesiseExcitation(std::vector<float>& excitation,
                                              int numSamples,
                                              float f0_hz,
                                              float& phase) {
    excitation.resize(numSamples);
    if (f0_hz <= 0.0f) {
        // Unvoiced: white noise
        for (int i = 0; i < numSamples; ++i) {
            // Simple LCG noise
            excitation[i] = (float(rand() % 32768) / 16384.0f) - 1.0f;
        }
        return;
    }

    const int maxHarmonic = static_cast<int>(sampleRate_ / 2.0 / f0_hz);
    const float phaseInc  = kTwoPi * f0_hz / float(sampleRate_);

    // Flat harmonic weighting (band-limited impulse train), NOT 1/k sawtooth.
    //
    // The spectral tilt of voiced speech is already carried by the rest of the
    // chain: the pre-emphasis path restores it via the output de-emphasis
    // filter, and the bypass path's LPC envelope (computed from the un-tilted
    // original frame) includes it.  A 1/k excitation added a SECOND −6 dB/oct
    // on top of either, making every LPC output ~15 dB too dark above 1 kHz
    // (measured on vocal pitch+7: 1–2 kHz at 26.7 dB vs reference 43.2 dB).
    for (int i = 0; i < numSamples; ++i) {
        float s = 0.0f;
        for (int k = 1; k <= maxHarmonic; ++k)
            s += std::sin(phase * float(k));
        excitation[i] = s;
        phase += phaseInc;
        if (phase > kTwoPi) phase -= kTwoPi;
    }

    // Normalise
    float peak = 0.0f;
    for (float v : excitation) peak = std::max(peak, std::abs(v));
    if (peak > 1e-6f)
        for (float& v : excitation) v /= peak;
}

// ─── LPC Synthesis Filter (all-pole IIR) ─────────────────────────────────────

void SourceFilterModel::synthesisFilter(const std::vector<float>& excitation,
                                         const std::vector<float>& coeffs,
                                         float gain,
                                         std::vector<float>& output) {
    const int N = static_cast<int>(excitation.size());
    output.resize(N);
    for (int n = 0; n < N; ++n) {
        float y = gain * excitation[n];
        for (int k = 0; k < kLPCOrder; ++k)
            y += coeffs[k] * filterState_[k];
        // Shift state (direct form II-transposed)
        for (int k = kLPCOrder - 1; k > 0; --k)
            filterState_[k] = filterState_[k - 1];
        filterState_[0] = y;
        output[n] = y;
    }
}

// ─── Cascade Biquad Synthesis Filter ─────────────────────────────────────────
//
// Processes excitation through kLPCOrder/2 all-pole biquad sections in series.
// Each section: y[n] = x[n] − b1·y[n−1] − b0·y[n−2]
//
// This is numerically equivalent to the direct-form IIR when all poles are
// well-separated, but avoids the float32 quantization problem that occurs when
// the n roots are multiplied back into a single n-th degree polynomial:
// near-coincident poles produce polynomial coefficients up to ±214 for a 16th-
// order filter, and float32 rounds those enough to push poles outside the unit
// circle.  Per-biquad coefficients are bounded ≤ 2 and remain well-conditioned.

void SourceFilterModel::synthesisFilterBiquad(const std::vector<float>& excitation,
                                               float gain,
                                               std::vector<float>& output) {
    const int N = static_cast<int>(excitation.size());
    const int M = static_cast<int>(biquads_.size());
    output.resize(N);

    // Grow state array if needed (shouldn't be necessary after prepare(),
    // but guards against being called before prepare() initialises biquadState_).
    if (static_cast<int>(biquadState_.size()) < M)
        biquadState_.assign(M, {0.0f, 0.0f});

    for (int n = 0; n < N; ++n) {
        float x = gain * excitation[n];
        for (int s = 0; s < M; ++s) {
            const float y = x
                          - biquads_[s].b1 * biquadState_[s][0]
                          - biquads_[s].b0 * biquadState_[s][1];
            biquadState_[s][1] = biquadState_[s][0];
            biquadState_[s][0] = y;
            x = y;
        }
        output[n] = x;
    }
}

// ─── Main Process ─────────────────────────────────────────────────────────────
//
// Frame-by-frame source-filter processing.
//
// For each analysis frame:
//   1. Detect F0 and voiced/unvoiced state
//   2. Compute LPC coefficients (formant filter)
//   3. Shift LPC poles for formant shift
//   4. Synthesise excitation at (pitch-shifted) F0
//   5. Run LPC synthesis filter
//   6. OLA into output ring buffer (time-stretch via variable synthesis hop)

void SourceFilterModel::processMono(const float* input, float* output, int numSamples) {
    const float pitchRatio   = std::pow(2.0f, params_.pitchShiftSemitones / 12.0f);
    const float timeStretch  = params_.timeStretchRatio;
    const float formantShift = params_.formantShiftSemitones;
    const bool hasFormantShiftBlock = (std::abs(formantShift) > 0.5f);

    const int inBufSize  = static_cast<int>(inputBuffer_.size());
    const int outBufSize = static_cast<int>(outputBuffer_.size());

    // ── 1. Buffer input ───────────────────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i) {
        inputBuffer_[inputWritePos_] = input[i];
        inputWritePos_ = (inputWritePos_ + 1) % inBufSize;
    }
    inputFill_ += numSamples;

    // ── 2. Process frames ─────────────────────────────────────────────────────
    while (inputFill_ >= kFrameSize) {

        // Extract windowed frame (unmodified — used for F0/voiced detection,
        // onset-RMS tracking, and as the reference for onset-blend pass-through).
        std::vector<float> frame(kFrameSize);
        {
            int rp = inputReadPos_;
            for (int i = 0; i < kFrameSize; ++i) {
                frame[i] = inputBuffer_[rp] * window_[i];
                rp = (rp + 1) % inBufSize;
            }
        }

        // Pre-emphasised windowed frame — used exclusively for LPC analysis.
        //
        // Pre-emphasis H(z) = 1 − 0.97z⁻¹ is applied to the raw (unwindowed)
        // input *before* multiplying by the Hann window.  For a 1/k harmonic-
        // series spectrum (voiced speech), |H(kω₀)|² ≈ 0.97(kω₀)² exactly
        // cancels the 1/k² amplitude roll-off, making every harmonic contribute
        // equally to the autocorrelation.  Without pre-emphasis the fundamental
        // (k=1) energy dominates by ~10 000× at 48 kHz, and Levinson-Durbin
        // clusters all available poles near F0 regardless of minGuardOrder.
        // With pre-emphasis the optimisation criterion is balanced across all
        // harmonics, so the predictor captures the formant envelope (F1–F4).
        //
        // The sample immediately before the analysis frame (from the ring buffer)
        // is used as the filter's initial state to avoid a step discontinuity at
        // the frame boundary.
        std::vector<float> frameEmph(kFrameSize);
        {
            float prevSample = inputBuffer_[(inputReadPos_ + inBufSize - 1) % inBufSize];
            int rp = inputReadPos_;
            for (int i = 0; i < kFrameSize; ++i) {
                const float raw = inputBuffer_[rp];
                frameEmph[i] = (raw - 0.97f * prevSample) * window_[i];
                prevSample = raw;
                rp = (rp + 1) % inBufSize;
            }
        }

        // F0 detection and voiced/unvoiced decision.
        // Uses the non-pre-emphasised windowed frame; estimateF0 applies its own
        // pre-emphasis internally for ACF peak detection.
        const bool voiced = isVoiced(frame);
        float f0 = voiced ? estimateF0(frame) : 0.0f;

        // Apply pitch shift to excitation F0
        const float excitationF0 = (f0 > 0.0f) ? f0 * pitchRatio : 0.0f;

        // LPC analysis on the pre-emphasised frame.
        //
        // minGuardOrder=8 is applied whenever the frame is voiced AND any
        // spectral-envelope manipulation is active (formant or pitch shift).
        // For formant shift: 8 poles guarantee at least F1–F4 are captured before
        //   Guard 3 (early-stop) is allowed to fire.
        // For pitch shift: 8 poles give a realistic formant envelope for the
        //   synthesis filter; the excitation F0 is then moved to the target pitch
        //   while the filter poles (vocal-tract formants) remain at their original
        //   frequencies.  With minGuardOrder=2 the 2-pole pitch model stays at F0
        //   even after the excitation moves, producing a phantom resonance at the
        //   original pitch — the root cause of the low vocal_aah_pitch_up7st score.
        //
        // Routing (Sessions 10–12 history):
        //   computeLPCCepstral (Session 10) was reverted — smooth spectrum still
        //   peaked at a pitch harmonic rather than F1.  Pre-emphasis on the
        //   analysis frame (Session 13) addresses the same problem differently:
        //   it changes the Levinson-Durbin cost function rather than the input
        //   signal, and works at 48 kHz provided minGuardOrder ≥ 8.
        const bool hasFormantShift = (std::abs(formantShift) > 0.5f);
        const bool hasPitchShift   = (std::abs(params_.pitchShiftSemitones) > 0.01f);

        // For non-speech formant-shift frames (pure sine, chord, drum), use the
        // unmodified analysis frame so the LPC analysis is in the same spectral
        // domain as the output (no pre-emphasis tilt to correct).  A 2 kHz bandpass
        // discriminant detects voiced speech: speech has F2/F3 energy there; a 440 Hz
        // sine and polyphonic chords do not.
        //
        // For all other cases — voiced speech formant shift, any pitch shift, time
        // stretch — use the pre-emphasised frame so Levinson-Durbin places poles at
        // formant frequencies rather than at the pitch harmonic.
        bool isVoicedSpeechFrame = false;
        if (hasFormantShiftBlock && voiced && (f0 > 0.0f)) {
            const double fc    = 2000.0;
            const double Q     = 1.5;
            const double omega = 2.0 * 3.14159265358979 * fc / sampleRate_;
            const double R     = 1.0 - (omega / (2.0 * Q));
            const double cosOm = std::cos(omega);
            const double bpG   = (1.0 - R * R) * 0.5;
            double totalE = 0.0, bpEnergy = 0.0;
            double xp2 = 0.0, xp1 = 0.0, yp2 = 0.0, yp1 = 0.0;
            for (int i = 0; i < kFrameSize; ++i) {
                const double x = double(frame[i]);
                totalE += x * x;
                const double y = bpG * (x - xp2)
                               + 2.0 * R * cosOm * yp1
                               - R * R * yp2;
                xp2 = xp1; xp1 = x;
                yp2 = yp1; yp1 = y;
                bpEnergy += y * y;
            }
            isVoicedSpeechFrame = (bpEnergy > 0.05 * totalE);
        }
        // usePreEmph is direction-aware in both axes:
        //   Formant shift: bypass pre-emphasis only for voiced non-speech frames with
        //   POSITIVE formant shift — pre-emphasis distorts upward pole shifts on pure
        //   tones (sine_440_formant_upmax: 32.8 plain vs 13.7 pre-emph) but helps
        //   downward shifts (+5.9 on sine_440_formant_downmax).
        //   Pitch shift: bypass pre-emphasis for large DOWNWARD pitch shifts — the
        //   shifted excitation F0 drops below the pre-emphasis corner so the tilted
        //   envelope mis-weights the low harmonics (vocal_aah_pitch_down12st:
        //   27.5 plain vs 19.9 pre-emph), while upward shifts benefit
        //   (vocal_aah_pitch_up7st: 19.0 plain vs 24.9 pre-emph).
        const bool largePitchDown = !hasFormantShiftBlock && voiced
                                    && (params_.pitchShiftSemitones < -6.0f);
        const bool formantUpBypass = hasFormantShiftBlock && voiced
                                     && (formantShift >= 0.0f);
        const bool usePreEmph = !(formantUpBypass || largePitchDown);
        const std::vector<float>& analysisFrame = usePreEmph ? frameEmph : frame;

        std::vector<float> lpcCoeffs;
        float lpcGain;

        // Standard LPC with voiced-adaptive minimum guard order.
        // minGuardOrder=8 for voiced speech with any spectral-envelope operation.
        const int minGuardOrder = (voiced && (hasFormantShift || hasPitchShift)) ? 8 : 2;
        computeLPC(analysisFrame, lpcCoeffs, lpcGain, minGuardOrder);

        // ── Bandwidth expansion (stability guard) ─────────────────────────────
        // Levinson-Durbin guarantees |reflection coefficients| < 1 in theory,
        // but floating-point arithmetic on near-periodic signals (pure sine)
        // can produce poles at or slightly outside the unit circle, causing
        // exponential blowup in the synthesis filter within a single frame.
        //
        // Solution: multiply a_k by λ^k (λ = 0.994) to shrink all poles radially
        // inward.  This adds ~0.6% bandwidth to each formant — perceptually
        // negligible — but guarantees |pole| ≤ 0.994^(1/p) < 1 for all p.
        // This is standard practice in LPC-based speech synthesis.
        {
            float lk = 0.994f;
            for (float& c : lpcCoeffs) { c *= lk; lk *= 0.994f; }
        }

        // Formant shift via pole angle scaling.
        // shiftFormants() populates biquads_ on success; we clear it first so
        // that a bail-out (wrong factor count) leaves biquads_ empty and the
        // code below falls back to direct-form synthesis.
        biquads_.clear();
        if (std::abs(formantShift) > 0.01f)
            shiftFormants(lpcCoeffs, formantShift);

        // Detect synthesis-mode switch (biquad ↔ direct-form).
        // Reset both filter states on switch to avoid feeding stale taps from
        // the previous representation into the new one.
        {
            const bool nowBiquad = !biquads_.empty();
            if (nowBiquad != useBiquad_) {
                std::fill(filterState_.begin(), filterState_.end(), 0.0f);
                for (auto& s : biquadState_) { s[0] = 0.0f; s[1] = 0.0f; }
                useBiquad_ = nowBiquad;
            }
        }

        // Synthesise excitation — OLA-coherent phase (Session 14 fix).
        //
        // Frames are OLA'd at synthHop (≈256-sample) intervals but each frame
        // is kFrameSize (1024) samples long.  Letting sawPhase_ advance by the
        // full frame per call meant overlapping frames carried excitation from
        // DIFFERENT time regions — unrelated phase in the overlap → heavy OLA
        // cancellation (≈10 dB level loss on a pure sine, smeared spectrum).
        // This was masked for 13 sessions by an output-ring bug that read
        // partial OLA sums; the Session 14 read-gating fix exposed it.
        //
        // Fix: synthesise the frame from a snapshot, then advance the running
        // phase by only the synthesis hop, so frame k+1 starts exactly where
        // it overlaps frame k on the OUTPUT timeline.
        std::vector<float> excitation;
        {
            const float hopAdvance = float(kHopSize) * timeStretch;
            float framePhase = sawPhase_;
            synthesiseExcitation(excitation, kFrameSize, excitationF0, framePhase);
            if (excitationF0 > 0.0f)
                sawPhase_ = std::fmod(sawPhase_
                            + kTwoPi * (excitationF0 / float(sampleRate_)) * hopAdvance,
                            kTwoPi);
            else
                sawPhase_ = framePhase;   // unvoiced: keep legacy behaviour
        }

        // Scale excitation by LPC gain (match input level)
        for (float& v : excitation) v *= lpcGain;

        // LPC synthesis filter — cascade biquad when formant shift is active
        // (avoids float32 quantization of the combined high-degree polynomial),
        // direct-form otherwise.
        std::vector<float> synthFrame;
        if (useBiquad_)
            synthesisFilterBiquad(excitation, 1.0f, synthFrame);
        else
            synthesisFilter(excitation, lpcCoeffs, 1.0f, synthFrame);

        // ── Per-frame energy normalization (all LPC paths) ──────────────────
        // Applied to BOTH biquad and direct-form synthesis paths.
        //
        // Why direct-form also needs normalization (discovered Session 8):
        //   The LPC gain (sqrt of prediction error) assumes the excitation is
        //   white noise.  In reality, the excitation is a band-limited sawtooth
        //   whose harmonics are not uniformly spread.  When a harmonic falls near
        //   a narrow-band resonance (pole at r=0.994 → Q ≈ 83), the filter
        //   amplifies that harmonic by ~83×, producing output RMS >> input RMS.
        //   For a voiced vowel pitch-shifted +7 st, the excitation harmonic
        //   nearest the original 2-pole filter resonance (~970 Hz) receives this
        //   amplification, yielding peaks of ~10× full-scale.
        //
        //   Per-frame RMS normalisation corrects the level regardless of the
        //   harmonic/pole coincidence, making the source-filter output always
        //   match the input frame's energy — exactly what V-Synth's source-filter
        //   synthesis does (confirmed by null-test level behaviour).
        //
        // Reference: Kleijn & Paliwal, "Speech Coding and Synthesis", Ch. 4.
        {
            // Output-domain normalisation (Session 15, take 2): target the
            // ORIGINAL frame energy, compared against what this synthFrame
            // becomes after de-emphasis (fresh-state IIR pass — inaccurate
            // for synthesis, fine for an energy estimate).  Matching
            // pre-emph-domain energies left the audible output ~10 dB quiet;
            // metric v2's gain-sensitive null test rejected this fix, but
            // metric v3 gain-matches, so per-frame level TRACKING (not just
            // global gain) is what counts now.
            float inputEnergy = 0.0f;
            for (float v : frame) inputEnergy += v * v;
            float synthEnergy = 0.0f;
            if (usePreEmph) {
                float s = 0.0f;
                for (float v : synthFrame) {
                    s = v + 0.97f * s;
                    synthEnergy += s * s;
                }
            } else {
                for (float v : synthFrame) synthEnergy += v * v;
            }

            if (synthEnergy > 1e-10f && inputEnergy > 1e-10f) {
                const float normGain = std::sqrt(inputEnergy / synthEnergy);
                for (float& v : synthFrame) v *= normGain;
            } else if (synthEnergy > 1e-10f) {
                // Input is silent — silence synthesis frame too
                for (float& v : synthFrame) v = 0.0f;
            }
        }

        // ── Onset detection — transient pass-through blend ────────────────────
        // When the input frame's RMS energy rises sharply relative to the
        // previous frame (transient onset), the synthesis frame is blended toward
        // a windowed copy of the input frame.  This preserves onset timing and
        // sharpness: the direct input signal has a sharp attack, while the
        // synthesised OLA signal has a gradual ramp-up that smears transients.
        //
        // Onset threshold: current RMS > kOnsetRatio × prevRMS.
        //   kOnsetRatio = 4.0f (≈12 dB sudden energy rise).
        //   onsetBlend  = 0.0 (synthesis only) → 1.0 (pass-through only).
        //
        // For sustained tones, this path is never taken.
        // For transient attacks (drum hits, consonants), it preserves the onset.
        {
            float curRMS = 0.0f;
            for (float v : frame) curRMS += v * v;
            curRMS = std::sqrt(curRMS / float(kFrameSize));

            constexpr float kOnsetRatio     = 4.0f;   // 12 dB rise → onset
            constexpr float kOnsetBlendMax  = 0.85f;  // max pass-through weight

            const bool onset = (prevFrameRMS_ > 1e-6f) &&
                               (curRMS > kOnsetRatio * prevFrameRMS_);

            if (onset) {
                // Blend synthesis frame toward the windowed input.
                // When de-emphasis is folded into the synthesis polynomial, synthFrame
                // is in the original (non-pre-emphasised) domain, so blend toward
                // the un-pre-emphasised frame.  Otherwise synthFrame is in the
                // pre-emphasised domain and frameEmph is the correct target.
                const float blend = kOnsetBlendMax *
                    std::min(1.0f, (curRMS - kOnsetRatio * prevFrameRMS_)
                                   / (curRMS + 1e-8f));
                const std::vector<float>& blendTarget = analysisFrame;
                for (int i = 0; i < kFrameSize; ++i) {
                    synthFrame[i] = (1.0f - blend) * synthFrame[i]
                                  +          blend  * blendTarget[i];
                }
                // Reset filter state: post-transient, start fresh so the filter
                // doesn't ring on old state that doesn't match the new segment.
                std::fill(filterState_.begin(), filterState_.end(), 0.0f);
                for (auto& s : biquadState_) { s[0] = 0.0f; s[1] = 0.0f; }
            }

            prevFrameRMS_ = curRMS;
        }

        // Apply output window for smooth OLA
        for (int i = 0; i < kFrameSize; ++i)
            synthFrame[i] *= window_[i];

        // OLA into the stream matching this frame's spectral domain:
        // pre-emphasised frames are de-emphasised at read time; bypassed frames
        // must not pass through that filter.
        //
        // Exception: polyphonic content (ENSEMBLE/BACKING).  Empirically, chord
        // formant shift scores best when bypassed frames ARE de-emphasised
        // (chord_Cmaj_formant_max: 21.1 tilted vs 17.9 clean vs 19.8 pre-emph) —
        // the low-shelf boost from 1/(1−0.97z⁻¹) better matches the V-Synth's
        // ENSEMBLE-mode output tilt.  Speech and pure tones want the clean split.
        const bool tiltBypassed = !usePreEmph && params_.polyphonicContent;
        std::vector<float>& olaTarget = (usePreEmph || tiltBypassed)
                                        ? outputBuffer_ : outputBufferPlain_;
        for (int i = 0; i < kFrameSize; ++i) {
            int pos = (outputWritePos_ + i) % outBufSize;
            olaTarget[pos] += synthFrame[i];
        }

        // Advance synthesis hop (time stretch)
        synthHopAccum_ += float(kHopSize) * timeStretch;
        int synthHop    = static_cast<int>(synthHopAccum_);
        synthHopAccum_ -= float(synthHop);
        outputWritePos_ = (outputWritePos_ + synthHop) % outBufSize;
        outputAvail_   += synthHop;

        // Advance analysis hop
        inputReadPos_ = (inputReadPos_ + kHopSize) % inBufSize;
        inputFill_   -= kHopSize;
    }

    // ── 3. Read output (with OLA normalization) ───────────────────────────────
    // Hann window OLA normalization: timeStretch / 2.0 (same derivation as PV)
    const float normFactor = timeStretch / 2.0f;

    // ── Read output (dual-stream de-emphasis) ─────────────────────────────────
    //
    // outputBuffer_ holds pre-emphasised-domain frames: de-emphasise with
    // 1/(1−0.97z⁻¹), continuous state across blocks.  outputBufferPlain_ holds
    // frames already in the signal domain: pass straight through.  Summing
    // after the filter keeps each frame in its correct spectral domain — the
    // old single-stream version tilted bypassed frames whenever any frame in
    // the block had used pre-emphasis.  When the pre-emph stream is silent the
    // filter just decays its state; no gating needed.
    // Read gating (Session 14): when the queue is empty, emit zeros without
    // advancing the read pointer — otherwise time compression makes the reader
    // overtake the writer and later frames are written behind it and lost.
    // lastValidOutput_ records the valid prefix for offline compaction.
    lastValidOutput_ = numSamples;
    for (int i = 0; i < numSamples; ++i) {
        if (outputAvail_ <= 0) {
            if (lastValidOutput_ == numSamples) lastValidOutput_ = i;
            output[i] = 0.0f;
            continue;
        }
        const float y_e = outputBuffer_[outputReadPos_] * normFactor;
        const float y   = y_e + 0.97f * deEmphState_;
        deEmphState_ = y;
        output[i]    = y + outputBufferPlain_[outputReadPos_] * normFactor;
        outputBuffer_[outputReadPos_]      = 0.0f;
        outputBufferPlain_[outputReadPos_] = 0.0f;
        outputReadPos_ = (outputReadPos_ + 1) % outBufSize;
        --outputAvail_;
    }
}

} // namespace VSE
