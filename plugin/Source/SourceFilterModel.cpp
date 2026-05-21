#include "SourceFilterModel.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <complex>

namespace VSE {

static constexpr float kTwoPi = 6.28318530718f;

// ─── Construction ─────────────────────────────────────────────────────────────

SourceFilterModel::SourceFilterModel() {
    window_.resize(kFrameSize);
    for (int i = 0; i < kFrameSize; ++i)
        window_[i] = 0.5f * (1.0f - std::cos(kTwoPi * float(i) / float(kFrameSize - 1)));
}

SourceFilterModel::~SourceFilterModel() = default;

void SourceFilterModel::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = sampleRate;

    inputBuffer_.assign(kFrameSize * 4, 0.0f);
    outputBuffer_.assign(kFrameSize * 16, 0.0f);
    lpcCoeffs_.assign(kLPCOrder, 0.0f);
    filterState_.assign(kLPCOrder, 0.0f);
    biquadState_.assign(kLPCOrder / 2, {0.0f, 0.0f});
    sawPhase_ = 0.0f;

    reset();
}

void SourceFilterModel::reset() {
    std::fill(inputBuffer_.begin(),  inputBuffer_.end(),  0.0f);
    std::fill(outputBuffer_.begin(), outputBuffer_.end(), 0.0f);
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
    synthHopAccum_  = 0.0f;
    sawPhase_       = 0.0f;
    lpcGain_        = 1.0f;
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
                                    float& gain) {
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
    //   For voiced speech, the order-2 residual is typically 10–60% of r[0] and
    //   the loop continues to the full kLPCOrder, capturing all formants.
    //
    // Threshold choice: 0.01 (stop when >99% of variance is explained).
    //   Catches pure/near-pure sinusoids (residual < 0.01%) without stopping
    //   early on complex spectra.
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
        // Minimum order 2: a pure sinusoid needs a conjugate pair (2 poles).
        // At order 1, the reflection coefficient for 440 Hz is cos(2π*440/44100)≈0.998,
        // giving error₁≈0.004×r[0] — already below the threshold.  Breaking at order 1
        // leaves only a single real (DC-ish) pole, which is useless for formant shift
        // and causes the biquad synthesis to produce near-DC-only output, wildly
        // amplified by energy normalization.  Requiring i≥2 ensures we always capture
        // at least one conjugate pair before declaring the model sufficient.
        if (i >= 2 && error < kEarlyStopThreshold * r[0]) break;
    }

    for (int i = 0; i < kLPCOrder; ++i)
        coeffs[i] = float(a[i + 1]);

    gain = float(std::sqrt(std::max(error, 1e-10)));
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

    // Pre-emphasised, windowed frame for ACF
    std::vector<float> x(N);
    for (int i = 1; i < N; ++i)
        x[i] = frame[i] - 0.97f * frame[i - 1];
    x[0] = frame[0];
    for (int i = 0; i < N; ++i)
        x[i] *= window_[i < static_cast<int>(window_.size()) ? i : 0];

    // Normalised autocorrelation
    const int minLag = static_cast<int>(sampleRate_ / 800.0);  // max ~800 Hz
    const int maxLag = static_cast<int>(sampleRate_ / 60.0);   // min ~60 Hz

    std::vector<float> acf(maxLag + 1, 0.0f);
    float r0 = 0.0f;
    for (int i = 0; i < N; ++i) r0 += x[i] * x[i];
    if (r0 < 1e-8f) return 0.0f;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        float sum = 0.0f;
        for (int i = 0; i < N - lag; ++i)
            sum += x[i] * x[i + lag];
        acf[lag] = sum / r0;
    }

    // Find best peak
    int bestLag = minLag;
    float bestVal = acf[minLag];
    for (int lag = minLag; lag <= maxLag; ++lag) {
        if (acf[lag] > bestVal) { bestVal = acf[lag]; bestLag = lag; }
    }

    if (bestVal < 0.3f) return 0.0f;   // unvoiced threshold

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

    for (int i = 0; i < numSamples; ++i) {
        float s = 0.0f;
        for (int k = 1; k <= maxHarmonic; ++k)
            s += (1.0f / float(k)) * std::sin(phase * float(k));
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

        // Extract windowed frame
        std::vector<float> frame(kFrameSize);
        {
            int rp = inputReadPos_;
            for (int i = 0; i < kFrameSize; ++i) {
                frame[i] = inputBuffer_[rp] * window_[i];
                rp = (rp + 1) % inBufSize;
            }
        }

        // F0 detection and voiced/unvoiced decision
        const bool voiced = isVoiced(frame);
        float f0 = voiced ? estimateF0(frame) : 0.0f;

        // Apply pitch shift to excitation F0
        const float excitationF0 = (f0 > 0.0f) ? f0 * pitchRatio : 0.0f;

        // LPC analysis
        std::vector<float> lpcCoeffs;
        float lpcGain;
        computeLPC(frame, lpcCoeffs, lpcGain);

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

        // Synthesise excitation
        std::vector<float> excitation;
        synthesiseExcitation(excitation, kFrameSize, excitationF0, sawPhase_);

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

        // ── Per-frame energy normalization (biquad path only) ────────────────
        // The LPC gain (sqrt of prediction error) correctly compensates for the
        // filter's spectral shape in the direct-form path — no extra normalisation
        // is needed or wanted there.
        //
        // In the biquad (formant-shifted) path the filter's gain at any given
        // frequency can change dramatically after the pole angle shift — e.g.
        // a -12 st shift on a pure 440 Hz sine clusters 16 poles near 220 Hz,
        // giving ~28000x gain even though the analytical stability is preserved.
        // Normalising only those frames keeps output levels consistent regardless
        // of how much the poles cluster after shifting, without affecting the
        // direct-form pitch/time cases where levels are already correct.
        //
        // Reference: Kleijn & Paliwal, "Speech Coding and Synthesis", Ch. 4.
        if (useBiquad_) {
            float inputEnergy = 0.0f;
            for (float v : frame)      inputEnergy += v * v;
            float synthEnergy = 0.0f;
            for (float v : synthFrame) synthEnergy += v * v;

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
                // Blend synthesis frame toward windowed input pass-through.
                // Use a softer blend on first onset (not a hard cut) to avoid
                // introducing clicks when the synthesis and input have
                // different levels.
                const float blend = kOnsetBlendMax *
                    std::min(1.0f, (curRMS - kOnsetRatio * prevFrameRMS_)
                                   / (curRMS + 1e-8f));
                for (int i = 0; i < kFrameSize; ++i) {
                    // frame[i] is already Hann-windowed
                    synthFrame[i] = (1.0f - blend) * synthFrame[i]
                                  +          blend  * frame[i];
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

        // OLA into output buffer
        for (int i = 0; i < kFrameSize; ++i) {
            int pos = (outputWritePos_ + i) % outBufSize;
            outputBuffer_[pos] += synthFrame[i];
        }

        // Advance synthesis hop (time stretch)
        synthHopAccum_ += float(kHopSize) * timeStretch;
        int synthHop    = static_cast<int>(synthHopAccum_);
        synthHopAccum_ -= float(synthHop);
        outputWritePos_ = (outputWritePos_ + synthHop) % outBufSize;

        // Advance analysis hop
        inputReadPos_ = (inputReadPos_ + kHopSize) % inBufSize;
        inputFill_   -= kHopSize;
    }

    // ── 3. Read output (with OLA normalization) ───────────────────────────────
    // Hann window OLA normalization: timeStretch / 2.0 (same derivation as PV)
    const float normFactor = timeStretch / 2.0f;

    for (int i = 0; i < numSamples; ++i) {
        output[i] = outputBuffer_[outputReadPos_] * normFactor;
        outputBuffer_[outputReadPos_] = 0.0f;
        outputReadPos_ = (outputReadPos_ + 1) % outBufSize;
    }
}

} // namespace VSE
