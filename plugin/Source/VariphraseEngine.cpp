#include "VariphraseEngine.h"
#include "PhaseVocoder.h"
#include "SourceFilterModel.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <atomic>
#include <mutex>
#include <string>
#include <cstdlib>
#include <cstdio>

namespace VSE {

// ─── Impl (PIMPL) ─────────────────────────────────────────────────────────────

struct VariphraseEngine::Impl {
    double sampleRate     = 44100.0;
    int    maxBlockSize   = 512;

    std::atomic<Algorithm> algorithm { Algorithm::Passthrough };

    // Parameters (written by audio thread owner; simple struct copy is safe at 32-bit alignment)
    VariphraseParams params {};

    // Algorithm implementations
    std::unique_ptr<PhaseVocoder>      phaseVocoder;
    std::unique_ptr<SourceFilterModel> sourceFilter;

    // Latency in samples (set by prepare())
    int latencySamples = 0;

    // Valid (non-starvation-padding) prefix of the last processMono output.
    // Set per call from the active processor; numSamples for passthrough.
    int lastValidOutput = 0;

    // ── Offline encode pass result ────────────────────────────────────────────
    // Set once by setAnalysis() before real-time processing starts.
    // Used by the Hybrid routing to decide WSOLA vs PV vs LPC.
    VariphraseAnalysis analysis {};

    // Debug
    VariphraseEngine::DebugCallback debugCallback;
    std::mutex debugMutex;

    void prepare(double sr, int blockSize) {
        sampleRate   = sr;
        maxBlockSize = blockSize;

        phaseVocoder = std::make_unique<PhaseVocoder>();
        phaseVocoder->prepare(sr, blockSize);

        sourceFilter = std::make_unique<SourceFilterModel>();
        sourceFilter->prepare(sr, blockSize);

        latencySamples = phaseVocoder->getLatencySamples();
    }

    void reset() {
        if (phaseVocoder) phaseVocoder->reset();
        if (sourceFilter) sourceFilter->reset();
    }

    // Parameters as the Hybrid router applies them.  Only SOLO and LITE
    // encodings have formant control on the V-Synth; ENSEMBLE and BACKING
    // ignore the formant knob (owner's manual, Session 8).  Confirmed on
    // the reference set (Session 16): chord_Cmaj_formant_max's reference
    // matches the dry input (spectral 0.935, harmonic 0.913) while our
    // formant-shifted render scored 0.785 / 0.097.
    VariphraseParams hybridParams() const {
        VariphraseParams p = params;
        if (analysis.contentType == VariphraseAnalysis::ContentType::ENSEMBLE ||
            analysis.contentType == VariphraseAnalysis::ContentType::BACKING)
            p.formantShiftSemitones = 0.0f;
        return p;
    }

    void processMono(const float* input, float* output, int numSamples) {
        const Algorithm algo = algorithm.load(std::memory_order_relaxed);
        lastValidOutput = numSamples;

        switch (algo) {
            case Algorithm::Passthrough:
                std::copy(input, input + numSamples, output);
                break;

            case Algorithm::PhaseVocoder:
                phaseVocoder->setParams(params);
                phaseVocoder->processMono(input, output, numSamples);
                lastValidOutput = phaseVocoder->getLastValidOutput();
                break;

            case Algorithm::SinusoidalPlusResidual:
                // TODO: Implement SMS model in Phase 3
                // For now fall through to phase vocoder
                phaseVocoder->setParams(params);
                phaseVocoder->processMono(input, output, numSamples);
                lastValidOutput = phaseVocoder->getLastValidOutput();
                break;

            case Algorithm::LPCSourceFilter:
                sourceFilter->setParams(params);
                sourceFilter->processMono(input, output, numSamples);
                lastValidOutput = sourceFilter->getLastValidOutput();
                break;

            case Algorithm::Hybrid: {
                // Shadows the member: formant zeroed for encode types without
                // formant control (see hybridParams).
                const VariphraseParams params = hybridParams();
                // ── Hybrid routing (Algorithm v5) ────────────────────────────
                // Updated Session 8: voiced-speech pitch shift now routes to LPC.
                //
                // V-Synth confirmed architecture: ALL VariPhrase operations use
                // source-filter.  Pitch shift moves only the excitation F0; the
                // formant filter is unchanged.  PV shifts ALL spectral content
                // including formants, causing a severe mismatch on voiced speech.
                //
                // Evidence (Session 7 vocal_aah batch):
                //   vocal_aah_pitch_up7st   PV = 10.0  (formant_sim 0.250) ← smoking gun
                //   vocal_aah_pitch_down12st PV = 18.3  (formant_sim 0.297) ← architecture gap
                //
                // SourceFilterModel already implements the correct behaviour:
                //   excitationF0 = f0 × pitchRatio   (excitation shifts)
                //   LPC filter poles unchanged         (formants stay)
                //
                // Routing rules (v5):
                //   hasFormant → LPC              (formant shift is LPC's specialty)
                //   hasPitch AND voiced → LPC     (pitch shift on voiced speech)
                //   else → PV                     (time-only, unvoiced, pure tones)
                //
                // Voiced-speech detection: ZCR < 0.15 AND energy > 1e-6.
                // A pure 440 Hz sine has ZCR ≈ 0.02 — also below 0.15 — so ZCR
                // alone cannot distinguish sines from speech.  We additionally
                // require that significant energy exists in the 1–4 kHz band,
                // which speech formants (F2–F3) occupy but a pure sine at 440 Hz
                // does not.  This protects sine_440 pitch-shift cases (PV wins by
                // 2–9 pts on those) while routing vocal cases to LPC.
                //
                // Band energy check: compare energy in [1 kHz, 4 kHz] relative to
                // total block energy.  For voiced "aah" this band has significant
                // energy from F2 (~1.2 kHz) and F3 (~2.5 kHz).  For a pure sine
                // at 440 Hz it is essentially zero.

                const bool hasFormant = (std::abs(params.formantShiftSemitones) > 0.5f);
                const bool hasPitch   = (std::abs(params.pitchShiftSemitones)   > 0.5f);

                // ── Voiced-speech detector (lightweight, per-block) ───────────
                // Gated by the offline encode-pass classification: only SOLO
                // content (monophonic voiced speech/melody) may route pitch
                // shifts to LPC.  BACKING (drums) and ENSEMBLE (chords) blocks
                // can transiently pass the ZCR + 2 kHz band check — C major
                // chord harmonics put >5% energy at 2 kHz — which routed some
                // chord_Cmaj_pitch blocks to LPC and cost 3.2 pts once LPC
                // pre-emphasis landed (Session 14).
                bool isVoicedSpeech = false;
                const bool soloContent =
                    (analysis.contentType == VariphraseAnalysis::ContentType::SOLO);
                if (hasPitch && !hasFormant && soloContent) {
                    // ZCR
                    int crossings = 0;
                    for (int i = 1; i < numSamples; ++i)
                        if ((input[i] >= 0.0f) != (input[i-1] >= 0.0f))
                            ++crossings;
                    const float zcr = float(crossings) / float(numSamples);

                    // Total energy
                    float energy = 0.0f;
                    for (int i = 0; i < numSamples; ++i) energy += input[i] * input[i];
                    energy /= float(numSamples);

                    if (zcr < 0.15f && energy > 1e-6f) {
                        // Band energy: 1–4 kHz via 2nd-order bandpass.
                        //
                        // Standard biquad bandpass:
                        //   H(z) = G × (1 - z^{-2}) / (1 - 2Rcos(ω₀)z^{-1} + R²z^{-2})
                        //
                        // The zeros at z=±1 (DC and Nyquist) come from (1-z^{-2}) applied
                        // to the INPUT signal x[n]-x[n-2], NOT to the output.  The earlier
                        // implementation incorrectly mixed input with output, producing an
                        // all-pole response that passed 440 Hz at ~54%, falsely triggering
                        // voiced detection for pure sines.
                        //
                        // Discriminant:
                        //   Vocal "aah" at ~120 Hz has significant energy from F2 (~1.2 kHz)
                        //   and F3 (~2.5 kHz).  The bandpass around 2 kHz captures this.
                        //   A pure 440 Hz sine has essentially no energy at 2 kHz.
                        const double fc    = 2000.0;
                        const double Q     = 1.5;
                        const double omega = 2.0 * 3.14159265358979 * fc / sampleRate;
                        const double R     = 1.0 - (omega / (2.0 * Q));
                        const double cosOm = std::cos(omega);
                        const double bpG   = (1.0 - R * R) * 0.5;  // peak gain ≈ 1/(1-R²)×bpG

                        double bpEnergy = 0.0;
                        double x_prev2 = 0.0, x_prev1 = 0.0; // x[n-2], x[n-1]
                        double y_prev2 = 0.0, y_prev1 = 0.0; // y[n-2], y[n-1]
                        for (int i = 0; i < numSamples; ++i) {
                            const double x = double(input[i]);
                            // y[n] = G*(x[n]-x[n-2]) + 2R*cos(ω₀)*y[n-1] - R²*y[n-2]
                            const double y = bpG * (x - x_prev2)
                                           + 2.0 * R * cosOm * y_prev1
                                           - R * R * y_prev2;
                            x_prev2 = x_prev1;  x_prev1 = x;
                            y_prev2 = y_prev1;  y_prev1 = y;
                            bpEnergy += y * y;
                        }
                        bpEnergy /= double(numSamples);

                        // Voiced speech if band energy is at least 5% of total energy.
                        // Pure 440 Hz sine: ~0%.  Vocal "aah": typically 15–40%.
                        // Drum hits: also high — but the LPC onset-blend handles them.
                        isVoicedSpeech = (bpEnergy > 0.05 * double(energy));
                    }
                }

                if (hasFormant || isVoicedSpeech) {
                    phaseVocoder->setForceWSOLA(false);
                    VariphraseParams lpcParams = params;
                    lpcParams.polyphonicContent =
                        (analysis.contentType == VariphraseAnalysis::ContentType::ENSEMBLE ||
                         analysis.contentType == VariphraseAnalysis::ContentType::BACKING);
                    sourceFilter->setParams(lpcParams);
                    sourceFilter->processMono(input, output, numSamples);
                    lastValidOutput = sourceFilter->getLastValidOutput();
                } else {
                    // Time-only (or pitch-only on non-voiced content) path.
                    //
                    // V-Synth encode pass: if the offline analysis classified
                    // this content as ENSEMBLE (polyphonic, chords) or BACKING
                    // (drums, transient-rich) AND we are doing a time-only
                    // stretch (no pitch, no formant), route to WSOLA.
                    //
                    // SOLO and LITE content stays on PV — sine_440 and vocal
                    // pitch/formant cases both score better with PV (see
                    // Session 7 evidence in the comment block above).
                    // WSOLA routing disabled (Session 15): after the v21 PV
                    // overhaul (inverted hops, identity phase locking, FFT
                    // sign fix) the phase vocoder beats WSOLA on every case
                    // that previously routed there — drum_hit_time_halfspeed,
                    // the last WSOLA case, scores 31.1 on PV vs 26.0 on WSOLA.
                    // processWSOLA() is retained in PhaseVocoder for reference.
                    //
                    // LITE content (pure tones) routes to the LPC source-filter
                    // even for time-only operations (Session 15): the V-Synth
                    // RESYNTHESIZES — its time-stretched sine reference carries
                    // strong sawtooth harmonics (880 Hz at −18 dB rel.) that a
                    // transparent PV stretch of the pure input can never have.
                    // sine_440_time_2x: 36.6 LPC vs 15.7 PV; pitch_up7st 43.1
                    // LPC vs 26.0 PV.  EXCEPT downward pitch: the LPC envelope
                    // (resonant at the original F0) boosts the wrong harmonic
                    // of the lowered excitation — pitch_down12st 6.1 LPC vs
                    // 48.8 PV.  SOLO (vocal) and BACKING/ENSEMBLE stay on PV.
                    if (analysis.contentType == VariphraseAnalysis::ContentType::LITE &&
                        params.pitchShiftSemitones > -0.5f) {
                        sourceFilter->setParams(params);
                        sourceFilter->processMono(input, output, numSamples);
                        lastValidOutput = sourceFilter->getLastValidOutput();
                    } else {
                        phaseVocoder->setForceWSOLA(false);
                        phaseVocoder->setParams(params);
                        phaseVocoder->processMono(input, output, numSamples);
                        lastValidOutput = phaseVocoder->getLastValidOutput();
                    }
                }
                break;
            }
        }
    }
};

// ─── VariphraseEngine ─────────────────────────────────────────────────────────

VariphraseEngine::VariphraseEngine()
    : pImpl(std::make_unique<Impl>()) {}

VariphraseEngine::~VariphraseEngine() = default;

void VariphraseEngine::prepare(double sampleRate, int maxBlockSize) {
    pImpl->prepare(sampleRate, maxBlockSize);
}

void VariphraseEngine::reset() {
    pImpl->reset();
}

void VariphraseEngine::process(const float* const* input,
                                float* const* output,
                                int numChannels,
                                int numSamples) {
    if (numChannels == 1) {
        pImpl->processMono(input[0], output[0], numSamples);
    } else if (numChannels >= 2) {
        // Stereo: process each channel independently
        // TODO: For better stereo coherence, process mid/side instead
        pImpl->processMono(input[0], output[0], numSamples);
        pImpl->processMono(input[1], output[1], numSamples);
    }
}

void VariphraseEngine::setParams(const VariphraseParams& params) {
    pImpl->params = params;
}

VariphraseParams VariphraseEngine::getParams() const {
    return pImpl->params;
}

void VariphraseEngine::setAlgorithm(Algorithm algo) {
    pImpl->algorithm.store(algo, std::memory_order_relaxed);
}

Algorithm VariphraseEngine::getAlgorithm() const {
    return pImpl->algorithm.load(std::memory_order_relaxed);
}

// ─── Pitch-Synchronous Granular Resynthesis (US6421642B1) ────────────────────
//
// The actual VariPhrase algorithm per Roland's playback patent (filed
// 2000-05-02; see research/PATENTS.md).  Encode cuts the phrase into
// ~one-pitch-period grains ("cut waveforms"), each with its measured pitch
// (cwp).  Playback:
//   TIME    — playing position pp advances by 1/timeStretch per output
//             sample; pp selects WHICH grain is current.
//   PITCH   — grains re-trigger at the target period ppw = cwp / pitchRatio,
//             on TWO channels offset by half a period, each with a
//             triangular window (offset triangles sum to a constant).
//   FORMANT — grain READ VELOCITY fsv: resampling the grain content scales
//             the spectral envelope.  Window length wl = cwp / fsv, CLAMPED
//             to ≤ ppw (the patent's clamp — the mechanism behind the
//             ~0.75× upward formant saturation measured in Session 15).

namespace {

struct GrainCut { double start; float cwp; bool voiced; int info = -1; };   // info: record info word (−1 = unknown)

// Encoder window lengths in samples at rate sr (1024 / 256 at 48 kHz).
int grainAcfWindow(double sr)   { return (int)std::lround(1024.0 * sr / 48000.0); }
int grainUnvoicedHop(double sr) { return (int)std::lround( 256.0 * sr / 48000.0); }

// Local pitch-period estimate around `start` — normalized ACF with an
// integer-subharmonic octave guard (same guards as SourceFilterModel's
// estimateF0, compact form).  prevPeriod, when > 0, narrows the search ±25 %
// for speed and continuity; a low-confidence result falls back to full range.
float estimateLocalPeriod(const std::vector<float>& x, int start, double sr,
                          float prevPeriod) {
    const int n = (int)x.size();
    // Window sizes are defined in time (tuned as 1024 / 256 samples at
    // 48 kHz) so the encoder behaves the same at any sample rate.
    const int w = std::min(grainAcfWindow(sr), n - start);
    if (w < grainUnvoicedHop(sr)) return 0.0f;

    int lagLo = std::max(2, (int)(sr / 500.0));
    int lagHi = std::min(w - 1, (int)(sr / 60.0));
    if (prevPeriod > 0.0f) {
        lagLo = std::max(lagLo, (int)(prevPeriod * 0.75f));
        lagHi = std::min(lagHi, (int)(prevPeriod * 1.25f));
        if (lagHi <= lagLo) { lagLo = std::max(2, (int)(sr / 500.0));
                              lagHi = std::min(w - 1, (int)(sr / 60.0)); }
    }

    double e0 = 0.0;
    for (int i = 0; i < w; ++i) e0 += double(x[start+i]) * x[start+i];
    if (e0 < 1e-9) return 0.0f;

    int bestLag = 0; double bestV = 0.0;
    for (int lag = lagLo; lag <= lagHi; ++lag) {
        double s = 0.0;
        for (int i = 0; i + lag < w; ++i)
            s += double(x[start+i]) * x[start+i+lag];
        const double v = (s / e0) * (double(w) / double(w - lag));  // unbiased
        if (v > bestV) { bestV = v; bestLag = lag; }
    }
    if (bestV < 0.3) {
        // Retry full range once before declaring unvoiced
        if (prevPeriod > 0.0f) return estimateLocalPeriod(x, start, sr, 0.0f);
        return 0.0f;
    }
    // Octave guard: prefer lag/2 if nearly as strong
    if (bestLag / 2 >= lagLo) {
        const int half = bestLag / 2;
        double s = 0.0;
        for (int i = 0; i + half < w; ++i)
            s += double(x[start+i]) * x[start+i+half];
        const double v = (s / e0) * (double(w) / double(w - half));
        if (v >= 0.95 * bestV) bestLag = half;
    }
    // Sub-sample period: parabolic interpolation on the ACF peak, quantised
    // to 1/16 sample — the V-Synth stores grain periods in 1/16-sample units
    // (firmware record format, research/HARDWARE_FIRMWARE.md pass 4).
    auto acf = [&](int lag) {
        double s = 0.0;
        for (int i = 0; i + lag < w; ++i) s += double(x[start+i]) * x[start+i+lag];
        return (s / e0) * (double(w) / double(w - lag));
    };
    double lag = bestLag;
    if (bestLag - 1 >= 2 && bestLag + 1 < w) {
        const double y0 = acf(bestLag - 1), y1 = acf(bestLag), y2 = acf(bestLag + 1);
        const double den = y0 - 2.0 * y1 + y2;
        if (den < -1e-12) {
            const double d = 0.5 * (y0 - y2) / den;
            if (std::abs(d) < 1.0) lag += d;
        }
    }
    return float(std::round(lag * 16.0) / 16.0);
}

// epochAlign: snap each voiced cut to the largest positive peak within
// ±period/4 of its predicted position, so cut boundaries sit on pitch
// epochs.  The firmware SOLO player centres every grain window on the next
// cut boundary (window peak at os + wl = C), which assumes epoch-aligned
// records; with alignment the firmware player gains +2.1 on the vocal set
// (formant downmax +7.4) while the patent-based engine loses −7.3 — the
// player and the record layout were evidently designed together.
std::vector<GrainCut> encodeGrainCuts(const std::vector<float>& x, double sr,
                                      bool epochAlign = false) {
    std::vector<GrainCut> cuts;
    const int n = (int)x.size();
    // Records are contiguous (firmware format: each record's length is the
    // period in 1/16-sample units), so grain starts accumulate the
    // fractional periods; analysis windows start at the nearest sample.
    const int hop = grainUnvoicedHop(sr);
    double pos = 0.0;
    float prev = 0.0f;
    while ((int)pos + hop < n) {
        const float p = estimateLocalPeriod(x, (int)std::lround(pos), sr, prev);
        if (p > 0.0f && epochAlign) {
            // Snap the cut to the largest positive peak within ±p/4 of the
            // predicted position (pitch-epoch alignment).
            const int c  = (int)std::lround(pos);
            const int lo = std::max(0, c - (int)(p / 4)), hi = std::min(n - 1, c + (int)(p / 4));
            int best = c;
            for (int i = lo; i <= hi; ++i) if (x[i] > x[best]) best = i;
            if (!cuts.empty()) pos = std::max(cuts.back().start + 2.0, (double)best);
            else pos = best;
        }
        if (p > 0.0f) {
            cuts.push_back({ pos, p, true });
            prev = p;
            pos += std::max(2.0f, p);
        } else {
            cuts.push_back({ pos, (float)hop, false });
            prev = 0.0f;
            pos += hop;
        }
    }
    return cuts;
}

// V-Trainer-style SOLO encoder (vpenc.dll, VP-9000 encoder shipped with
// V-Producer; research/HARDWARE_FIRMWARE.md "V-Trainer encoder").
//   voicing  : D/E < kVoicedErr, D = Σ (x[i−L] − x[i])² and E = Σ x[i]²
//              over one period (template 0: tbr = 3 → (1 − 0.3)/2 = 0.35)
//   tracking : from mark m with period L, hill-climb the lag minimising D
//              (window fixed at L), parabolic refinement, next mark m + L'
//   epoch    : first mark of a voiced run = centre of the max-energy
//              window of length L/4 within the first period (+L/8)
//   range    : 22–2000 Hz (template pmin/pmax); unvoiced records ≈ 1 ms
// Ported: run-level octave correction (0x10008da0 vote rule).  Not yet
// ported: the 11.025 kHz decimated tracker, the candidate-list onset search
// (0x100085d0), the backward run extension.
std::vector<GrainCut> encodeGrainCutsVTrainer(const std::vector<float>& x, double sr) {
    constexpr double kVoicedErr = 0.35;
    std::vector<GrainCut> cuts;
    const int n = (int)x.size();
    const double Lmin = std::max(2.0, sr / 2000.0);
    const double Lmax = std::min(sr / 22.0, 2047.0 * sr / 44100.0);
    const int unvoicedLen = std::max(8, (int)std::lround(sr / 1000.0));
    const int searchHop   = std::max(16, (int)std::lround(421.0 * sr / 44100.0));

    auto D = [&](double m, int lag, int len) {
        const int b = (int)std::lround(m);
        double d = 0.0;
        for (int i = 0; i < len; ++i) {
            const int k = b + i;
            if (k - lag < 0 || k >= n) return 1e30;
            const double e = (double)x[k - lag] - (double)x[k];
            d += e * e;
        }
        return d;
    };
    auto E = [&](double m, int len) {
        const int b = (int)std::lround(m);
        double e = 0.0;
        for (int i = 0; i < len && b + i < n; ++i) e += (double)x[b + i] * x[b + i];
        return e;
    };
    // Hill-climb + parabolic refinement of the lag around L0, window len.
    auto track = [&](double m, double L0, double& err) {
        const int len = std::max(2, (int)std::lround(L0));
        int L = (int)std::lround(L0);
        L = std::clamp(L, (int)std::ceil(Lmin), (int)std::floor(Lmax));
        double dL = D(m, L, len);
        for (int dir : {+1, -1}) {
            while (true) {
                const int Ln = L + dir;
                if (Ln < Lmin || Ln > Lmax) break;
                const double dn = D(m, Ln, len);
                if (!(dn < dL)) break;
                L = Ln; dL = dn;
            }
        }
        double Lf = L;
        const double d0 = D(m, L - 1, len), d2 = D(m, L + 1, len);
        const double den = d0 - 2.0 * dL + d2;
        if (den > 1e-20 && d0 < 1e29 && d2 < 1e29) {
            const double off = 0.5 * (d0 - d2) / den;
            if (std::abs(off) < 1.0) Lf += off;
        }
        const double e = E(m, len);
        err = (e > 1e-12) ? dL / e : 1e9;
        return std::round(Lf * 16.0) / 16.0;    // 1/16-sample record units
    };
    auto epochIn = [&](double start, double L) {
        const int w = std::max(1, (int)std::lround(L / 4.0));
        const int b = (int)std::lround(start), P = (int)std::lround(L);
        if (b + P + w >= n) return start;
        double acc = 0.0;
        for (int i = 0; i < w; ++i) acc += (double)x[b + i] * x[b + i];
        double best = acc; int bi = 0;
        for (int i = 1; i < P; ++i) {
            acc += (double)x[b + i + w - 1] * x[b + i + w - 1] - (double)x[b + i - 1] * x[b + i - 1];
            if (acc > best) { best = acc; bi = i; }
        }
        int c = bi + (int)std::lround(L / 8.0);
        while (c >= P) c -= P;
        return start + c;
    };

    double pos = 0.0;
    while ((int)pos + searchHop < n) {
        // Onset search: candidate period from the ACF estimator, verified
        // with the period-difference test one period later.
        const float pc = estimateLocalPeriod(x, (int)std::lround(pos), sr, 0.0f);
        bool voiced = false;
        double L = pc, err = 1e9;
        if (pc > 0.0f && pos + 3 * pc < n) {
            L = track(pos + pc, pc, err);
            voiced = err < kVoicedErr;
        }
        if (!voiced) {
            // Unvoiced: fill this hop with ~1 ms records.
            const double end = std::min((double)n, pos + searchHop);
            while (pos + unvoicedLen <= end) {
                cuts.push_back({ pos, (float)unvoicedLen, false });
                pos += unvoicedLen;
            }
            if (pos < end) { cuts.push_back({ pos, (float)(end - pos), false }); pos = end; }
            continue;
        }
        // Voiced run: align the first mark to the energy epoch, then step.
        const double ep = epochIn(pos, L);
        if (ep > pos + 0.5) { cuts.push_back({ pos, (float)(ep - pos), false }); }
        std::vector<std::pair<double,double>> run;     // (mark, period)
        double m = ep;
        while (m + L < n - 1) {
            double e2;
            const double Ln = track(m + L, L, e2);
            if (e2 >= kVoicedErr) break;
            run.push_back({ m, L });
            m += L;
            L = Ln;
        }
        run.push_back({ m, L });
        // Octave correction (0x10008da0): up to 32 sample points; P/k votes
        // when D(P/k) ≤ D(P) or (D(P/k) − D(P))/E < oru/2 (oru = 0.04);
        // the largest k with votes·32 ≥ checks·9 splits every interval.
        constexpr double kOru = 0.04;
        int bestK = 1;
        {
            const size_t R = run.size(), step = std::max<size_t>(1, R / 32);
            int checks = 0, votes[7] = {0};
            for (size_t i = 0; i < R; i += step) {
                const double mm = run[i].first, P = run[i].second;
                const int len = std::max(2, (int)std::lround(P));
                const double dP = D(mm + P, len, len), eP = E(mm + P, len);
                if (dP > 1e29 || eP < 1e-12) continue;
                ++checks;
                for (int k = 2; k <= 6; ++k) {
                    const int lk = (int)std::lround(P / k);
                    if (lk < Lmin) break;
                    const double dk = D(mm + P, lk, len);
                    if (dk <= dP || (dk - dP) / eP < kOru * 0.5) ++votes[k];
                }
            }
            for (int k = 2; k <= 6; ++k)
                if (checks > 0 && votes[k] * 32 >= checks * 9) bestK = k;
        }
        for (const auto& [mk, P] : run) {
            const double sub = P / bestK;
            for (int j = 0; j < bestK; ++j)
                cuts.push_back({ mk + j * sub, (float)(std::round(sub * 16.0) / 16.0), true });
        }
        pos = run.back().first + run.back().second;
    }
    return cuts;
}

} // namespace

// ─── Subband Time Stretch (US6564187B1, simplified) ─────────────────────────
//
// Roland's polyphonic time-stretch patent: octave-cascade bands, each split
// into sub-bands, per-sub-band amplitude + instantaneous frequency, time
// modification of those trajectories, resynthesis.  This implementation uses
// the offline luxury of a whole-file FFT: quarter-octave log-spaced sub-bands
// (≈ one partial per band for musical content — the reason the patent's
// sub-division works), analytic signal per band, amplitude/inst-frequency
// trajectories resampled to the stretched timeline, cosine-bank resynthesis.
// Raised-cosine band masks sum to unity, so stretch = 1 reconstructs the
// input (verified).

namespace {

void engFft(std::vector<std::complex<double>>& d, bool inverse) {
    const int N = (int)d.size();
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(d[i], d[j]);
    }
    for (int len = 2; len <= N; len <<= 1) {
        const double ang = 2.0 * 3.14159265358979 / len * (inverse ? -1.0 : 1.0);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int j2 = 0; j2 < len / 2; ++j2) {
                const auto u = d[i + j2];
                const auto v = d[i + j2 + len / 2] * w;
                d[i + j2]            = u + v;
                d[i + j2 + len / 2]  = u - v;
                w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : d) x /= (double)N;
}

} // namespace

std::vector<float> VariphraseEngine::subbandStretchOffline(
        const std::vector<float>& in, float stretch, float pitchRatio) const {
    const int    n  = (int)in.size();
    const double sr = pImpl->sampleRate;
    const double st = std::max(0.05, (double)stretch);
    const double pr = std::max(0.05, (double)pitchRatio);
    if (n < 4096) return {};

    int N = 1; while (N < n) N <<= 1;
    std::vector<std::complex<double>> spec(N, {0.0, 0.0});
    for (int i = 0; i < n; ++i) spec[i] = { (double)in[i], 0.0 };
    engFft(spec, false);

    // Quarter-octave band edges from 55 Hz to Nyquist (in bins).
    std::vector<double> edges { 0.0, 55.0 };
    while (edges.back() * std::pow(2.0, 0.25) < sr / 2.0)
        edges.push_back(edges.back() * std::pow(2.0, 0.25));
    edges.push_back(sr / 2.0);

    const int outLen = std::max(1, (int)std::lround(n * st));
    std::vector<double> out(outLen, 0.0);
    const double binHz = sr / (double)N;

    std::vector<std::complex<double>> band(N);
    for (size_t b = 0; b + 1 < edges.size(); ++b) {
        const double fLo = edges[b], fHi = edges[b + 1];
        const double tw  = 0.15 * (fHi - fLo);   // raised-cosine transition
        // One-sided analytic band: positive frequencies only, ×2
        std::fill(band.begin(), band.end(), std::complex<double>(0.0, 0.0));
        bool any = false;
        for (int k = 0; k <= N / 2; ++k) {
            const double f = k * binHz;
            double g = 0.0;
            if (f >= fLo + tw && f <= fHi - tw)      g = 1.0;
            else if (f > fLo - tw && f < fLo + tw)   g = 0.5 * (1.0 + std::sin(3.14159265358979 * (f - fLo) / (2.0 * tw)));
            else if (f > fHi - tw && f < fHi + tw)   g = 0.5 * (1.0 - std::sin(3.14159265358979 * (f - fHi) / (2.0 * tw)));
            if (g > 0.0) {
                const double scale = (k == 0 || k == N / 2) ? 1.0 : 2.0;
                band[k] = spec[k] * (g * scale);
                any = true;
            }
        }
        if (!any) continue;
        engFft(band, true);   // analytic band signal z(t)

        // Amplitude + instantaneous frequency trajectories, resampled to the
        // stretched timeline; phase rebuilt by integrating the resampled
        // frequency (per-band running phase seeded from the source phase).
        double phase = std::arg(band[0]);
        for (int t = 0; t < outLen; ++t) {
            const double srcPos = (double)t / st;
            const int    i0     = std::min((int)srcPos, n - 2);
            const double fr     = srcPos - i0;

            const std::complex<double> z0 = band[i0], z1 = band[i0 + 1];
            const double a = (1.0 - fr) * std::abs(z0) + fr * std::abs(z1);
            // Instantaneous angular frequency from the analytic pair
            const std::complex<double> rot = z1 * std::conj(z0);
            // Pitch shift (subband method): scale each band's instantaneous
            // frequency; amplitude timeline unchanged.
            const double w = ((std::abs(rot) > 1e-30) ? std::arg(rot) : 0.0) * pr;

            out[t] += a * std::cos(phase);
            phase  += w;
            if (phase >  3.14159265358979) phase -= 2.0 * 3.14159265358979;
            if (phase < -3.14159265358979) phase += 2.0 * 3.14159265358979;
        }
    }

    std::vector<float> res(outLen);
    for (int t = 0; t < outLen; ++t) res[t] = (float)out[t];
    return res;
}

std::vector<float> VariphraseEngine::granularResynthOffline(
        const std::vector<float>& in) const {
    const int n = (int)in.size();
    const double sr = pImpl->sampleRate;
    const float stretch    = std::max(0.05f, pImpl->params.timeStretchRatio);
    const float pitchRatio = std::pow(2.0f, pImpl->params.pitchShiftSemitones / 12.0f);
    const float fsv        = std::pow(2.0f, pImpl->params.formantShiftSemitones / 12.0f);

    const std::vector<GrainCut> cuts = encodeGrainCuts(in, sr);
    if (cuts.size() < 2) return {};

    const int outLen = std::max(1, (int)std::lround((double)n * stretch));
    std::vector<float> out(outLen, 0.0f);

    // One active grain per channel (wl ≤ ppw guarantees no within-channel
    // overlap).  age is in output samples since trigger.
    struct Grain { double srcStart = 0, wl = 0, age = 1e18; };
    Grain ch[2];
    double nextTrig[2] = { 0.0, -1.0 };   // ch2 initialised at first ppw/2

    const double ppInc = 1.0 / stretch;   // playing-position advance per output sample
    double pp = 0.0;
    size_t cutIdx = 0;

    for (int t = 0; t < outLen; ++t, pp += ppInc) {
        // Track the current cut from the playing position
        while (cutIdx + 1 < cuts.size() && pp >= (double)cuts[cutIdx + 1].start)
            ++cutIdx;
        const GrainCut& cut = cuts[cutIdx];
        const double ppw = std::max(2.0, (double)cut.cwp / (double)pitchRatio);
        if (nextTrig[1] < 0.0) nextTrig[1] = ppw * 0.5;

        for (int c = 0; c < 2; ++c) {
            if ((double)t >= nextTrig[c]) {
                const double wlFull = (double)cut.cwp / (double)fsv;
                ch[c].wl  = std::min(wlFull, ppw);
                // Patent read offset (US6421642): when the window clamps to
                // ppw, the grain reads the LAST ppw×fsv samples of the cut
                // (os = cwp − ppw·fsv), not the first — the read region is
                // anchored to the cut END so the covered content stays
                // centred as ppw shrinks (pitch-up).
                const double os = (wlFull > ppw)
                    ? (double)cut.cwp - ppw * (double)fsv
                    : 0.0;
                ch[c].srcStart = (double)cut.start + os;
                ch[c].age = 0.0;
                nextTrig[c] += ppw;
            }
            if (ch[c].age < ch[c].wl) {
                const double srcPos = ch[c].srcStart + ch[c].age * (double)fsv;
                const int    i0     = (int)srcPos;
                if (i0 >= 0 && i0 + 1 < n) {
                    const double frac = srcPos - i0;
                    const double s    = in[i0] * (1.0 - frac) + in[i0 + 1] * frac;
                    // Triangular window 0→1→0 over wl
                    const double ph = ch[c].age / ch[c].wl;
                    const double wv = 1.0 - std::abs(2.0 * ph - 1.0);
                    out[t] += (float)(s * wv);
                }
                ch[c].age += 1.0;
            }
        }
    }
    return out;
}

std::vector<float> VariphraseEngine::periodJumpResynthOffline(
        const std::vector<float>& in) const {
    // Transcription of the firmware player 0x8950e1e0 (per output sample):
    //   P  += 1/stretch                         phrase walk (time)
    //   L   = cur + (P − segStart)/cur·(next − cur)   period, interpolated
    //                                           toward the next grain record
    //   Q  += pitchRatio                        read pointer (resampling)
    //   P' = clamp(P, L, n − L)
    //   if Q > P'       Q −= L                  stay within one period behind
    //   if Q + L ≤ P'   Q += L
    //   w   = 5·(clamp((P − Q)/L, 0.4, 0.6) − 0.4)
    //   out = s(Q) + (s(Q+L) − s(Q))·w           two taps one period apart
    // s() is 2-point linear interpolation at 1/16-sample resolution, as in
    // the firmware (positions there are 1/16-sample fixed point).
    const int n = (int)in.size();
    const double sr = pImpl->sampleRate;
    const double stretch    = std::max(0.05, (double)pImpl->params.timeStretchRatio);
    const double pitchRatio = std::pow(2.0, pImpl->params.pitchShiftSemitones / 12.0);

    const std::vector<GrainCut> cuts = encodeGrainCuts(in, sr);
    if (cuts.size() < 2) return {};
    // Grain records: contiguous segments, length = distance to the next cut.
    std::vector<double> segStart(cuts.size()), segLen(cuts.size());
    for (size_t i = 0; i < cuts.size(); ++i) {
        segStart[i] = cuts[i].start;
        const double end = (i + 1 < cuts.size()) ? cuts[i + 1].start : (double)n;
        segLen[i] = std::max(2.0, end - cuts[i].start);
    }

    auto tap = [&](double pos) -> double {
        const double q  = std::floor(pos * 16.0) / 16.0;   // 1/16-sample grid
        const int    i0 = (int)std::floor(q);
        if (i0 < 0 || i0 + 1 >= n) return 0.0;
        const double fr = q - i0;
        return in[i0] + (in[i0 + 1] - in[i0]) * fr;
    };

    const int outLen = std::max(1, (int)std::lround((double)n * stretch));
    std::vector<float> out(outLen, 0.0f);
    const double walk = 1.0 / stretch;
    double P = 0.0, Q = 0.0;
    size_t seg = 0;
    for (int t = 0; t < outLen; ++t) {
        while (seg + 1 < cuts.size() && P >= segStart[seg + 1]) ++seg;
        const double cur  = segLen[seg];
        const double next = (seg + 1 < cuts.size()) ? segLen[seg + 1] : cur;
        const double L = cur + (P - segStart[seg]) / cur * (next - cur);

        const double Pc = std::clamp(P, L, std::max(L, (double)n - 1.0 - L));
        if (Q > Pc)      Q -= L;
        if (Q + L <= Pc) Q += L;

        const double lag = (P - Q) / L;
        const double w   = 5.0 * (std::clamp(lag, 0.4, 0.6) - 0.4);
        const double s1  = tap(Q);
        out[t] = (float)(s1 + (tap(Q + L) - s1) * w);

        P += walk;
        Q += pitchRatio;
    }
    return out;
}

std::vector<float> VariphraseEngine::soloFirmwareResynthOffline(
        const std::vector<float>& in) const {
    // Units: the firmware works in 1/16-sample fixed point; here everything
    // is in samples (floats), which is the same arithmetic without the
    // quantisation.  C = current grain period, N = next grain's period.
    //
    // Per output sample:
    //   P += 1/stretch                       phrase walk (time)        [ce80]
    //   segment = record containing P; frac = (P − segStart)/C          [d5a0]
    //   timer += 1; if timer ≥ ppw: timer −= ppw, TRIGGER on the other channel
    //   each channel: e = min(e,1); w = 2·min(e, 1−e); e += 0.5/wl     [d380]
    //                 pos = start + age·v; s = lerp(pos);
    //                 s += (lerp(pos + Ctap) − s)·fracTap;  age += 1   [d0f0]
    //   out = wA·sA + wB·sB → highpass SVF (Energy OFF: fc = f0/8, q = 1.2)
    // TRIGGER (voiced record, firmware info bit 0x4 with pitch ratio A):
    //   Ci  = C + frac·(N − C)               interpolated period
    //   ppw = Ci / A                         re-trigger spacing (pitch)
    //   v   = F                              read velocity (formant)
    //   wl  = Ci / F; if wl > ppw: os = Ci − ppw·F, wl = ppw, else os = 0
    //   start = segStart + os; age = timer remainder; e = remainder·0.5/wl
    // Unvoiced records (no bit 0x4): S = A (formant gate off) — ppw = Ci/S,
    //   v = S, wl = ppw, os = 0 (plain pitch resampling of noise grains).
    const int n = (int)in.size();
    const double sr = pImpl->sampleRate;
    const double stretch = std::max(0.05, (double)pImpl->params.timeStretchRatio);
    const double A = std::pow(2.0, pImpl->params.pitchShiftSemitones / 12.0);
    const double F = std::clamp(std::pow(2.0, pImpl->params.formantShiftSemitones / 12.0),
                                0.5, 2.0);   // firmware clamps the formant ratio to [0.5, 2]

    // VSE_ENC=vt selects the V-Trainer-style encoder (experiment).  Session
    // 16 on the vocal set: 64.2 vs 65.6 for epoch-snapped cuts (V-Trainer
    // aligns only each voiced run's first mark to the energy epoch, then
    // steps by the tracked period; re-anchoring every mark to the energy
    // epoch scored 62.5).  The VP-9000 encoder may differ from the V-Synth's.
    const char* encSel = std::getenv("VSE_ENC");
    std::vector<GrainCut> cuts = (encSel && std::string(encSel) == "vt")
        ? encodeGrainCutsVTrainer(in, sr)
        : encodeGrainCuts(in, sr, /*epochAlign=*/true);
    // Diagnostic: VSE_CUTSFILE = "start len voiced" lines (e.g. the real
    // V-Trainer records parsed from a VP-9000 sample's vpdt chunk).
    if (const char* cf = std::getenv("VSE_CUTSFILE")) {
        if (FILE* f = std::fopen(cf, "r")) {
            std::vector<GrainCut> ext;
            char line[256];
            while (std::fgets(line, sizeof line, f)) {
                double st, len; int v, info = -1;
                const int k = std::sscanf(line, "%lf %lf %d %d", &st, &len, &v, &info);
                if (k >= 3) ext.push_back({ st, (float)len, v != 0, k == 4 ? info : -1 });
            }
            std::fclose(f);
            if (ext.size() >= 2) cuts = ext;
        }
    }
    if (cuts.size() < 2) return {};
    const size_t nc = cuts.size();
    if (const char* dump = std::getenv("VSE_DUMPCUTS")) {   // diagnostic: start len voiced
        if (FILE* f = std::fopen(dump, "w")) {
            for (size_t i = 0; i < nc; ++i) {
                const double end = (i + 1 < nc) ? cuts[i + 1].start : (double)in.size();
                std::fprintf(f, "%.4f %.4f %d\n", cuts[i].start, end - cuts[i].start, (int)cuts[i].voiced);
            }
            std::fclose(f);
        }
    }
    if (std::getenv("VSE_DEBUG")) {
        size_t nv = 0; double sumL = 0, voicedSpan = 0;
        for (size_t i = 0; i < nc; ++i) if (cuts[i].voiced) { ++nv; sumL += cuts[i].cwp; }
        for (size_t i = 0; i + 1 < nc; ++i) if (cuts[i].voiced) voicedSpan += cuts[i+1].start - cuts[i].start;
        std::fprintf(stderr, "cuts %zu voiced %zu meanL %.2f voiced-span %.0f of %d samples\n",
                     nc, nv, nv ? sumL / nv : 0.0, voicedSpan, (int)in.size());
    }
    std::vector<double> segStart(nc), segLen(nc);
    for (size_t i = 0; i < nc; ++i) {
        segStart[i] = cuts[i].start;
        const double end = (i + 1 < nc) ? cuts[i + 1].start : (double)n;
        segLen[i] = std::max(10.0, end - cuts[i].start);   // firmware floor: 160/16 samples
    }

    auto lerp = [&](double pos) -> double {
        const int i0 = (int)std::floor(pos);
        if (i0 < 0 || i0 + 1 >= n) return 0.0;
        const double fr = pos - i0;
        return in[i0] + (in[i0 + 1] - in[i0]) * fr;
    };

    struct Chan { double start = 0, age = 0, v = 1, e = 1.0, slope = 0, tap = 0, tapW = 0; };
    Chan ch[2];
    int next = 0;

    const int outLen = std::max(1, (int)std::lround((double)n * stretch));
    std::vector<float> out(outLen, 0.0f);
    const double walk = 1.0 / stretch;
    double P = 0.0, timer = 1e9, ppw = segLen[0];
    size_t seg = 0;
    // Record selection.  Default: the record containing P.  VSE_ANCHOR:
    // firmware scheduler (0x8950ce80 / d6f0) — an anchor walks the source at
    // max(rate,1); when it passes the record end while P is still inside,
    // the record REPEATS if its budget ((info & 0x1F8) << 6; unknown info or
    // last record = unlimited) covers the repeat cost, else playback moves
    // on to the next record; records P has already passed are skipped.
    const bool anchorMode = std::getenv("VSE_ANCHOR") != nullptr;
    double anchor = 0.0, anchorStep = std::max(A, 1.0);
    auto budgetOf = [&](size_t i) -> double {
        if (i + 1 >= nc || cuts[i].info < 0) return 1e30;          // unlimited
        return (double)((cuts[i].info & 0x1F8) << 6);
    };
    double budget = budgetOf(0);
    auto repeatCost = [&](double a, double t) {
        double x = (a < 1.0 ? (3.0 - 2.0 * a) : (1.5 - 0.5 * a));
        x = std::max(0.0, x) * std::abs(t) * 8192.0;
        return x >= 2048.0 ? x : std::max(0.0, 2.0 * x - 2048.0);
    };
    // Energy-OFF highpass (Chamberlin SVF), coefficients smoothed per
    // 32-sample block as in 0x8950cde0.
    double low = 0, band = 0, fCoef = 0, q = 1.2;
    // SOLO "Energy" (manual p. 92). The firmware's g = voice param +0x78;
    // its knob scaling (OFF/1–127 → g) is not yet decoded.  Experiment hook.
    double energy = 0.0;
    if (const char* e = std::getenv("VSE_ENERGY")) energy = std::atof(e);

    for (int t = 0; t < outLen; ++t) {
        if (!anchorMode) {
            while (seg + 1 < nc && P >= segStart[seg + 1]) ++seg;
        } else {
            anchor += anchorStep;
            const double segEnd = segStart[seg] + segLen[seg];
            if (anchor >= segEnd) {
                const double cost = repeatCost(anchorStep, walk);
                if (P < segEnd && budget > 0.0 && budget >= cost) {
                    if (budget < 1e29) budget -= cost;
                    while (anchor >= segStart[seg] + segLen[seg]) anchor -= segLen[seg];
                } else if (seg + 1 < nc) {
                    ++seg; budget = budgetOf(seg);
                    while (seg + 1 < nc && budget > 0.0 && P >= segStart[seg] + segLen[seg]) {
                        anchor += segLen[seg];
                        ++seg; budget = budgetOf(seg);
                    }
                }
            }
        }
        const double C = segLen[seg];
        const double N = (seg + 1 < nc) ? segLen[seg + 1] : C;
        const double frac = std::clamp((P - segStart[seg]) / C, 0.0, 1.0);

        timer += 1.0;
        if (timer >= ppw) {
            timer = std::min(timer - ppw, ppw);
            const double Ci = C + frac * (N - C);
            bool voiced = cuts[seg].voiced;
            if (const char* lm = std::getenv("VSE_LITE"); lm && std::string(lm) == "solo-np") voiced = false;
            // EXPERIMENT: VSE_LITEINFO=b forces the record info low bits:
            // bit 0x4 → patent path; else non-patent with S = A'·F',
            // A' = (b & 1 ? A : 1), F' = (b & 2 ? F : 1)   (0x8950d5a0).
            int forcedInfo = -1;
            if (const char* fi = std::getenv("VSE_LITEINFO")) { forcedInfo = std::atoi(fi); voiced = (forcedInfo & 4) != 0; }
            double v, wl, os = 0.0;
            if (voiced) {
                ppw = Ci / A;
                v   = F;
                wl  = Ci / F;
                if (wl > ppw) { os = Ci - ppw * F; wl = ppw; }
            } else {
                double S = A;
                if (forcedInfo >= 0) S = ((forcedInfo & 1) ? A : 1.0) * ((forcedInfo & 2) ? F : 1.0);
                ppw = Ci / S;
                v   = S;
                wl  = ppw;
            }
            anchorStep = std::max(voiced ? A : v, 1.0);
            Chan& c  = ch[next];
            next ^= 1;
            c.start  = segStart[seg] + os;
            c.age    = timer;
            c.v      = v;
            c.slope  = 0.5 / wl;
            c.e      = timer * c.slope;
            c.tap    = C;
            c.tapW   = frac;
        }

        double y = 0.0;
        for (Chan& c : ch) {
            const double e = std::min(c.e, 1.0);
            const double w = 2.0 * std::min(e, 1.0 - e);
            if (w > 0.0) {
                const double pos = c.start + c.age * c.v;
                double s = lerp(pos);
                if (c.tapW > 0.0) s += (lerp(pos + c.tap) - s) * c.tapW;
                y += w * s;
            }
            c.e = e + c.slope;
            c.age += 1.0;
        }

        if ((t & 31) == 0) {   // per-block coefficient update        [cde0]
            // Energy g = 0 (OFF): cutoff f0/8, damping 1.2 (rumble filter).
            // Energy g > 0: cutoff at the output fundamental, damping
            // (1.2 − g) + p(rate) with p = g·(2r − 1) on [0.5, 1),
            // g·(2 − r) on [1, 2), 0 outside — neutral at no pitch shift,
            // strongest resonance at ≥ an octave of shift.
            const double r = A;
            double x = std::min(1.0 / ppw, 0.5), qt;
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
        low += fCoef * band;
        const double high = y - q * band - low;
        band += fCoef * high;
        out[t] = (float)high;

        P += walk;
    }
    return out;
}

std::vector<float> VariphraseEngine::processOffline(const std::vector<float>& inputMono) {
    // ── Pitch-synchronous granular for SOLO (Session 15, US6421642) ──────────
    // The patent engine handles all three axes in one mechanism.  SOLO only:
    // measured on the suite, granular lifts every vocal case (formant_upmax
    // +16.3, spectral sim 0.29-0.41 → 0.38-0.56) but drops every sine case
    // (formant_upmax −38.6) — consistent with Roland's "LITE" being the
    // reduced-analysis mode that does NOT run the full phrase engine.  LITE
    // keeps the v23 routing (LPC resynthesis / PV).
    if (pImpl->algorithm.load(std::memory_order_relaxed) == Algorithm::Hybrid &&
        pImpl->analysis.contentType == VariphraseAnalysis::ContentType::SOLO) {
        // v31 (Session 16): the firmware SOLO player is the default.  Vocal
        // cases vs the patent-based granular engine (metric v6): time ×½
        // 64.1 → 69.1, time 2× 63.6 → 67.7, pitch −12 63.3 → 70.0, formant
        // up 61.1 → 62.9 / +4 56.2 → 58.8; formant downmax 58.9 → 55.3,
        // pitch +7 63.8 → 61.2.  The time-only gains (no knob-mapping
        // assumptions involved) validate the transcribed structure.
        // VSE_SOLO=patent selects the older engine for comparison.
        const char* m = std::getenv("VSE_SOLO");
        if (!(m && std::string(m) == "patent")) {
            std::vector<float> fw = soloFirmwareResynthOffline(inputMono);
            if (!fw.empty()) return fw;
        }
        std::vector<float> out = granularResynthOffline(inputMono);
        if (!out.empty()) return out;
    }

    const VariphraseParams hp = pImpl->hybridParams();

    // EXPERIMENT (LITE player search): VSE_LITE = solo-p | solo-np | pj
    // routes LITE content to the firmware SOLO player (patent / non-patent
    // record path) or the period-jump player.
    if (const char* lm = std::getenv("VSE_LITE");
        lm && pImpl->algorithm.load(std::memory_order_relaxed) == Algorithm::Hybrid &&
        pImpl->analysis.contentType == VariphraseAnalysis::ContentType::LITE) {
        const std::string m(lm);
        std::vector<float> out;
        if (m == "pj") out = periodJumpResynthOffline(inputMono);
        else if (m == "solo-p" || m == "solo-np") out = soloFirmwareResynthOffline(inputMono);
        if (!out.empty()) return out;
    }

    // ── Subband pitch for ENSEMBLE (Session 16, US6564187) ───────────────────
    // Per-band instantaneous-frequency scaling.  Rejected in Session 15
    // (39.6 vs 40.1 PV) under metric v4, whose phase-sensitive gain made the
    // chord score noise.  Under v6: chord_Cmaj_pitch_up7st 66.8 → 71.6
    // (spectral 0.879 → 0.892, pitch 0.510 → 0.689).  The PV path dropped the
    // transposed C and E of the chord entirely (only G survived, plus an
    // unshifted 523 Hz partial).  The firmware period-jump player was also
    // tried here: it reproduces all three notes at reference levels but adds
    // splice sidebands (−30…−40 dB) a tempered chord cannot avoid and the
    // reference does not show — scored 52.3.
    if (pImpl->algorithm.load(std::memory_order_relaxed) == Algorithm::Hybrid &&
        pImpl->analysis.contentType == VariphraseAnalysis::ContentType::ENSEMBLE &&
        std::abs(hp.pitchShiftSemitones) >= 0.01f) {
        std::vector<float> out = subbandStretchOffline(
            inputMono, hp.timeStretchRatio,
            std::pow(2.0f, hp.pitchShiftSemitones / 12.0f));
        if (!out.empty()) return out;
    }

    // EXPERIMENT: route to the firmware period-jump player instead of the
    // defaults.  VSE_PERIODJUMP letters: B = BACKING pitch, D = BACKING
    // time-only, T = ENSEMBLE time-only.  Session 16 (metric v6): B +2.3,
    // D +1.0 / −3.6 / −0.5, T −20.7.  Not adopted for BACKING until the drum
    // references are verified (they look mis-recorded — see notes.txt).
    if (const char* pj = std::getenv("VSE_PERIODJUMP");
        pj && pImpl->algorithm.load(std::memory_order_relaxed) == Algorithm::Hybrid) {
        const std::string mode(pj);
        const auto ct = pImpl->analysis.contentType;
        const bool hasPitch = std::abs(hp.pitchShiftSemitones) >= 0.01f;
        const bool hasTime  = std::abs(hp.timeStretchRatio - 1.0f) > 0.01f;
        const bool ens = ct == VariphraseAnalysis::ContentType::ENSEMBLE;
        const bool bak = ct == VariphraseAnalysis::ContentType::BACKING;
        if ((ens && !hasPitch && hasTime && mode.find('T') != std::string::npos) ||
            (bak && hasPitch && mode.find('B') != std::string::npos) ||
            (bak && !hasPitch && hasTime && mode.find('D') != std::string::npos)) {
            std::vector<float> out = periodJumpResynthOffline(inputMono);
            if (!out.empty()) return out;
        }
    }

    // ── Subband stretch for ENSEMBLE time-only (Session 15, US6564187) ───────
    // Pitch via per-band inst-frequency scaling was tested (chord_pitch_up7st
    // 39.6 vs 40.1 PV — a wash) and not adopted; ENSEMBLE pitch stays on PV.
    {
        const bool timeOnlyEns =
            std::abs(hp.pitchShiftSemitones)   < 0.01f &&
            std::abs(hp.formantShiftSemitones) < 0.001f &&
            std::abs(pImpl->params.timeStretchRatio - 1.0f) > 0.01f;
        if (pImpl->algorithm.load(std::memory_order_relaxed) == Algorithm::Hybrid &&
            pImpl->analysis.contentType == VariphraseAnalysis::ContentType::ENSEMBLE &&
            timeOnlyEns) {
            std::vector<float> out = subbandStretchOffline(
                inputMono, pImpl->params.timeStretchRatio, 1.0f);
            if (!out.empty()) return out;
        }
    }

    // ── Event-based stretch for BACKING content (Session 15) ─────────────────
    // V-Synth BACKING mode stores onset stamps at encode time and keeps the
    // transients intact under time stretch: each attack is placed VERBATIM at
    // its stretched output position; only the decay/silence between events is
    // actually time-stretched.  Time-only operations on BACKING content with
    // detected onsets take this path; everything else falls through to the
    // normal streaming render below.
    {
        const float st = pImpl->params.timeStretchRatio;
        const bool timeOnly =
            std::abs(hp.pitchShiftSemitones)   < 0.01f &&
            std::abs(hp.formantShiftSemitones) < 0.001f;
        if (pImpl->analysis.contentType == VariphraseAnalysis::ContentType::BACKING &&
            timeOnly && std::abs(st - 1.0f) > 0.01f &&
            !pImpl->analysis.onsetSamples.empty()) {

            const int n = (int)inputMono.size();
            const int outLenTotal = std::max(1, (int)std::lround((double)n * st));
            std::vector<float> out(outLenTotal, 0.0f);

            // Helper: stretch a vector to an exact target length via the PV.
            auto pvStretch = [&](const std::vector<float>& seg, int targetLen)
                    -> std::vector<float> {
                if (seg.empty() || targetLen <= 0) return {};
                VariphraseParams p {};
                p.timeStretchRatio = float(targetLen) / float(seg.size());
                pImpl->phaseVocoder->setParams(p);
                pImpl->phaseVocoder->reset();
                pImpl->phaseVocoder->setOutputCapacity(targetLen + 8192);
                pImpl->phaseVocoder->setForceWSOLA(false);

                std::vector<float> res;
                res.reserve(targetLen + 1024);
                const int bs = pImpl->maxBlockSize;
                std::vector<float> inBuf(bs), outBuf(bs);
                int fed = 0;
                const int maxIters = 4 * ((int)seg.size() + targetLen) / bs + 64;
                for (int it = 0; it < maxIters &&
                       (fed < (int)seg.size() || (int)res.size() < targetLen); ++it) {
                    std::fill(inBuf.begin(), inBuf.end(), 0.0f);
                    if (fed < (int)seg.size()) {
                        const int c = std::min(bs, (int)seg.size() - fed);
                        std::copy(seg.begin() + fed, seg.begin() + fed + c, inBuf.begin());
                        fed += c;
                    }
                    pImpl->phaseVocoder->processMono(inBuf.data(), outBuf.data(), bs);
                    const int valid = std::min(pImpl->phaseVocoder->getLastValidOutput(), bs);
                    if (valid > 0)
                        res.insert(res.end(), outBuf.begin(), outBuf.begin() + valid);
                }
                res.resize(targetLen, 0.0f);
                return res;
            };

            // Segment boundaries: [0, onset_1, onset_2, …, n]
            std::vector<int> bounds { 0 };
            for (int e : pImpl->analysis.onsetSamples)
                if (e > bounds.back() && e < n) bounds.push_back(e);
            if (bounds.back() != n) bounds.push_back(n);

            constexpr int kAttackLen = 2048;   // ≈43 ms at 48 kHz, kept verbatim
            constexpr int kXfade     = 128;

            for (int b = 0; b + 1 < (int)bounds.size(); ++b) {
                const int s = bounds[b], e = bounds[b + 1];
                const int segLen   = e - s;
                const int outStart = (int)std::lround((double)s * st);
                const int outEnd   = std::min(outLenTotal, (int)std::lround((double)e * st));
                const int outLen   = outEnd - outStart;
                if (segLen <= 0 || outLen <= 0) continue;

                // Segments that begin at a detected onset keep their attack
                // verbatim; the leading pre-onset segment has no attack.
                const bool atOnset  = (b > 0);
                const int attackLen = atOnset ? std::min({ kAttackLen, segLen, outLen })
                                              : 0;
                for (int i = 0; i < attackLen; ++i)
                    out[outStart + i] = inputMono[s + i];

                const int tailInLen  = segLen - attackLen;
                const int tailOutLen = outLen - attackLen;
                if (tailInLen > 0 && tailOutLen > 0) {
                    std::vector<float> tail(inputMono.begin() + s + attackLen,
                                            inputMono.begin() + e);
                    // Subband-stretched tails (v28): +3 measured standalone on
                    // drum_time_2x vs PV tails.  Falls back to PV for segments
                    // too short for the whole-file-FFT subband method.
                    std::vector<float> stretched = subbandStretchOffline(
                        tail, float(tailOutLen) / float(tail.size()), 1.0f);
                    if ((int)stretched.size() < tailOutLen)
                        stretched = pvStretch(tail, tailOutLen);
                    stretched.resize(tailOutLen, 0.0f);
                    for (int i = 0; i < tailOutLen; ++i) {
                        float v = stretched[i];
                        // Short crossfade out of the verbatim attack
                        if (attackLen > 0 && i < kXfade) {
                            const float w = float(i) / float(kXfade);
                            v = w * v + (1.0f - w) * out[outStart + attackLen + i];
                        }
                        out[outStart + attackLen + i] = v;
                    }
                }
            }
            return out;
        }
    }

    // Session 14 rework: drain mode for time stretch.
    //
    // The old implementation collected exactly inputLen output samples.  Two
    // problems for timeStretch ≠ 1:
    //   1. The stretched signal is inputLen×stretch long; the tail was lost.
    //   2. For stretch > 1 the engines' OLA write pointer advances stretch×
    //      faster than the read pointer.  The surplus grows to
    //      inputLen×(stretch−1), lapping the default 64k output ring after
    //      ~1.4 s at 48 kHz and corrupting the OLA sum (confirmed: the
    //      vocal_aah_time_halfspeed render degraded at exactly 1.37 s).
    //
    // Fix: pre-size the engine output rings to the full render, feed all the
    // input, then keep pumping zero-input blocks ("drain") until the expected
    // inputLen×stretch samples (plus latency) have been collected.  Trailing
    // zero blocks are harmless: their Hann-windowed OLA contribution is zero,
    // and the queued surplus drains through the normal read path.
    const int   blockSize = pImpl->maxBlockSize;
    const int   n         = static_cast<int>(inputMono.size());
    const float stretch   = std::max(0.05f, pImpl->params.timeStretchRatio);
    const int   expected  = std::max(1, (int)std::lround((double)n * (double)stretch));

    if (pImpl->phaseVocoder) pImpl->phaseVocoder->setOutputCapacity(expected + 8 * blockSize);
    if (pImpl->sourceFilter) pImpl->sourceFilter->setOutputCapacity(expected + 8 * blockSize);

    std::vector<float> output;
    output.reserve(expected + blockSize);

    int fed = 0;
    // Safety cap: enough iterations to feed all input AND collect all output
    // (the valid prefix per call can be < blockSize, so allow generous slack).
    const int maxIters = 4 * ((n + expected) / blockSize) + 64;
    for (int iter = 0;
         (fed < n || (int)output.size() < expected) && iter < maxIters;
         ++iter) {
        std::vector<float> inBuf(blockSize, 0.0f);
        if (fed < n) {
            const int count = std::min(blockSize, n - fed);
            std::copy(inputMono.begin() + fed, inputMono.begin() + fed + count,
                      inBuf.begin());
            fed += count;
        }

        std::vector<float> outBuf(blockSize, 0.0f);
        const float* inPtrs[1]  = { inBuf.data() };
        float*       outPtrs[1] = { outBuf.data() };
        process(inPtrs, outPtrs, 1, blockSize);

        // Keep only the valid (non-starvation-padding) prefix.  Read gating in
        // the processors means engine latency zeros and compression starvation
        // gaps are reported as invalid rather than collected — so no separate
        // latency strip is needed and the stream is contiguous signal.
        const int valid = std::min(pImpl->lastValidOutput, blockSize);
        if (valid > 0)
            output.insert(output.end(), outBuf.begin(), outBuf.begin() + valid);

        // After all input is fed, stop once the engines run dry.
        if (fed >= n && valid == 0 && iter > (n / blockSize) + 8 &&
            (int)output.size() >= expected)
            break;
    }

    output.resize(expected);
    return output;
}

int VariphraseEngine::getLatencySamples() const {
    return pImpl->latencySamples;
}

void VariphraseEngine::setDebugCallback(DebugCallback cb) {
    std::lock_guard<std::mutex> lock(pImpl->debugMutex);
    pImpl->debugCallback = std::move(cb);
}

// ─── Offline Encode Pass ──────────────────────────────────────────────────────
//
// analyzeContent — V-Synth-style "encode" step.
//
// Computes a per-frame unbiased ACF pitch confidence over the full input
// buffer, then classifies the content type based on median confidence and
// the peak-to-mean energy ratio (transient indicator):
//
//   medianConf > 0.95  →  single-pitch content
//     + 1–4 kHz band energy > 5% of total  →  SOLO  (voiced speech / melody)
//     + otherwise                           →  LITE  (pure tone / oscillator)
//
//   medianConf ≤ 0.95  →  polyphonic or transient-rich
//     + peakToMeanEnergy > 5.0  →  BACKING   (drums / transient-rich)
//     + otherwise               →  ENSEMBLE  (chords / polyphonic)
//
// The thresholds are derived from the ACF confidence distributions measured
// in Session 10:
//   vocal_aah  p10=0.848  median=0.980   (SOLO — above 0.95 in median)
//   chord_Cmaj p10=0.000  median=0.685   (ENSEMBLE — below 0.95 in median)
//   sine_440   p10=1.000  median=1.001   (LITE — above 0.95 in median)
//   drum_hit   p10=0.324  median=0.713   (BACKING — below 0.95 with high peak/mean)
//
// ACF parameters:
//   kFrameSize = 2048  (same as kFFTSize — gives correct biased→unbiased scaling
//                       for F0=130 Hz: biased≈0.80, unbiased≈0.984)
//   kStep      = 512   (one analysis hop between frames)
//   lagLo      = sr/500 ≈  88  (500 Hz upper F0 limit)
//   lagHi      = sr/60  ≈ 735  (60 Hz lower F0 limit)
//
// Unbiased ACF normalisation: multiply biased ACF[lag] by N/(N-lag).
// Then normalise by ACF[0] to get a confidence in [0, ~1].

VariphraseAnalysis VariphraseEngine::analyzeContent(const float* mono,
                                                     int           numSamples,
                                                     double        sampleRate) {
    VariphraseAnalysis result;
    if (numSamples < 2048 || sampleRate <= 0.0) return result;

    const int kFrameSize = 2048;
    const int kStep      = 512;
    const int lagLo = std::max(1, static_cast<int>(sampleRate / 500.0));  //  ~88 @ 44.1kHz
    const int lagHi = std::min(kFrameSize - 1,
                               static_cast<int>(sampleRate / 60.0));       // ~735 @ 44.1kHz

    std::vector<float> confValues;
    std::vector<float> rmsValues;

    // ── Per-frame unbiased ACF confidence ─────────────────────────────────────
    for (int start = 0; start + kFrameSize <= numSamples; start += kStep) {
        const float* frame = mono + start;

        // RMS energy — skip silent frames (below −80 dBFS equivalent)
        float energy = 0.0f;
        for (int i = 0; i < kFrameSize; ++i) energy += frame[i] * frame[i];
        energy /= float(kFrameSize);
        const float rms = std::sqrt(energy);
        if (rms < 1e-4f) continue;
        rmsValues.push_back(rms);

        // Biased ACF at lag 0 (= sum of squares = energy × N)
        const float acf0 = energy * float(kFrameSize);
        if (acf0 < 1e-10f) continue;

        // Find max unbiased ACF confidence over [lagLo, lagHi]
        float maxConf = 0.0f;
        for (int lag = lagLo; lag <= lagHi; ++lag) {
            float sum = 0.0f;
            for (int i = 0; i + lag < kFrameSize; ++i)
                sum += frame[i] * frame[i + lag];
            // Unbiased: biased × N/(N-lag), normalised by acf0
            const float unbiased = sum * float(kFrameSize)
                                 / (float(kFrameSize - lag) * acf0);
            if (unbiased > maxConf) maxConf = unbiased;
        }
        confValues.push_back(maxConf);
    }

    if (confValues.empty()) return result;  // all silent — return default LITE

    // ── Median pitch confidence ───────────────────────────────────────────────
    std::vector<float> sorted = confValues;
    std::sort(sorted.begin(), sorted.end());
    result.medianPitchConf = sorted[sorted.size() / 2];

    // ── Peak-to-mean energy ratio (transient indicator) ───────────────────────
    if (!rmsValues.empty()) {
        float sumRms = 0.0f;
        float peakRms = 0.0f;
        for (float r : rmsValues) {
            sumRms += r;
            if (r > peakRms) peakRms = r;
        }
        result.peakToMeanEnergy = peakRms / (sumRms / float(rmsValues.size()));
    }

    // ── Content type classification ───────────────────────────────────────────
    if (result.medianPitchConf > 0.95f) {
        // High median ACF confidence → single-pitch content.
        // Distinguish SOLO (voiced speech) from LITE (pure tone) via band energy:
        //   voiced speech has significant formant energy in 1–4 kHz;
        //   a pure 440 Hz sine has essentially no energy above ~500 Hz.
        const double fc    = 2000.0;
        const double Q     = 1.5;
        const double omega = 2.0 * 3.14159265358979 * fc / sampleRate;
        const double R     = 1.0 - (omega / (2.0 * Q));
        const double cosOm = std::cos(omega);
        const double bpG   = (1.0 - R * R) * 0.5;

        double bpEnergy    = 0.0;
        double totalEnergy = 0.0;
        double x_prev2 = 0.0, x_prev1 = 0.0;
        double y_prev2 = 0.0, y_prev1 = 0.0;

        const int analyzed = std::min(numSamples, static_cast<int>(sampleRate)); // first second
        for (int i = 0; i < analyzed; ++i) {
            const double x = double(mono[i]);
            const double y = bpG * (x - x_prev2)
                           + 2.0 * R * cosOm * y_prev1
                           - R * R * y_prev2;
            x_prev2 = x_prev1;  x_prev1 = x;
            y_prev2 = y_prev1;  y_prev1 = y;
            bpEnergy    += y * y;
            totalEnergy += x * x;
        }

        const bool hasFormantEnergy = (totalEnergy > 1e-10 &&
                                       bpEnergy > 0.05 * totalEnergy);
        result.contentType = hasFormantEnergy
            ? VariphraseAnalysis::ContentType::SOLO
            : VariphraseAnalysis::ContentType::LITE;

    } else {
        // Low median ACF confidence → polyphonic or transient-rich.
        // Threshold raised 5.0 → 9.0 (Session 15): a strummed chord's attack
        // gives peakToMean ≈ 7.8 while a drum hit gives ≈ 11.4.  The chord is
        // polyphonic sustained content (ENSEMBLE), and classifying it BACKING
        // would route it through event-based stretching meant for percussion.
        result.contentType = (result.peakToMeanEnergy > 9.0f)
            ? VariphraseAnalysis::ContentType::BACKING
            : VariphraseAnalysis::ContentType::ENSEMBLE;
    }

    // ── Event stamps (BACKING) ────────────────────────────────────────────────
    // Onset = short-window RMS rising > 3× above the recent local maximum,
    // above an absolute floor.  Used by the offline event-based stretch to
    // place attacks verbatim at their stretched positions (V-Synth BACKING
    // mode stores exactly this at encode time).
    if (result.contentType == VariphraseAnalysis::ContentType::BACKING) {
        const int hop = 256;
        std::vector<float> env;
        for (int s = 0; s + hop <= numSamples; s += hop) {
            float e = 0.0f;
            for (int i = s; i < s + hop; ++i) e += mono[i] * mono[i];
            env.push_back(std::sqrt(e / hop));
        }
        float peak = 0.0f;
        for (float v : env) peak = std::max(peak, v);
        const float floorRms = 0.05f * peak;
        int lastOnset = -100000;
        for (int i = 4; i < (int)env.size(); ++i) {
            float prevMax = 0.0f;
            for (int k = i - 4; k < i; ++k) prevMax = std::max(prevMax, env[k]);
            if (env[i] > floorRms && env[i] > 3.0f * std::max(prevMax, 1e-6f) &&
                (i * hop - lastOnset) > 2048) {
                result.onsetSamples.push_back(i * hop);
                lastOnset = i * hop;
            }
        }
    }

    return result;
}

void VariphraseEngine::setAnalysis(const VariphraseAnalysis& analysis) {
    pImpl->analysis = analysis;
}

} // namespace VSE
