#pragma once

#include "VariphraseEngine.h"
#include <vector>
#include <complex>

namespace VSE {

/**
 * PhaseVocoder — VariPhrase time/pitch/formant processor.
 *
 * Implements independent control of:
 *   - Time stretch  — phase vocoder OLA with variable synthesis hop (default path)
 *                     OR time-domain WSOLA when forceWSOLA_ is set AND timeStretch ≥ 1×
 *   - Pitch shift   — stretch by pitchRatio, then streaming linear resample back
 *   - Formant shift — cepstral-liftered spectral envelope warp in frequency domain
 *
 * Ring-buffer streaming design: processMono() accepts arbitrary block sizes.
 *
 * Routing (controlled by caller via setForceWSOLA):
 *   forceWSOLA_ = false (default)  → phase vocoder (all content)
 *   forceWSOLA_ = true,
 *     no pitch shift, no formant shift,
 *     timeStretch ≥ 1.0            → WSOLA (time-domain OLA with similarity search)
 *   forceWSOLA_ = true but pitch/formant active OR timeStretch < 1.0
 *                                  → falls back to phase vocoder
 *
 * forceWSOLA_ is set by VariphraseEngine after the offline encode pass classifies
 * content as ENSEMBLE or BACKING (V-Synth terminology).  It is never set from within
 * this class based on per-frame analysis.
 *
 * WSOLA time-compression limitation: synthesis hop < analysis hop → output read drains
 * the OLA buffer faster than write fills it → silence.  Callers must only set
 * forceWSOLA_ when timeStretch ≥ 1.0.
 *
 * References:
 *   - Laroche & Dolson (1999), "Improved Phase Vocoder Time-Scale Modification"
 *   - Bonada (2000), "Automatic technique in frequency domain for near-lossless TSM"
 *   - Verhelst & Roelands (1993), "An overlap-add technique based on waveform
 *     similarity (WSOLA) for high quality time-scale modification of speech"
 */
class PhaseVocoder {
public:
    PhaseVocoder();
    ~PhaseVocoder();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setParams(const VariphraseParams& params);
    void processMono(const float* input, float* output, int numSamples);

    int getLatencySamples() const;

private:
    // ── FFT parameters ──────────────────────────────────────────────────────
    static constexpr int kFFTSize = 2048;
    static constexpr int kHopSize = 512;            // analysis hop (fixed)
    static constexpr int kOverlap = kFFTSize / kHopSize;   // 4

    // ── State ────────────────────────────────────────────────────────────────
    double sampleRate_ = 44100.0;
    VariphraseParams params_ {};

    // ── Input ring buffer ────────────────────────────────────────────────────
    std::vector<float> inputBuffer_;
    int inputWritePos_ = 0;   // next slot to write incoming samples
    int inputReadPos_  = 0;   // next slot to read for analysis
    int inputFill_     = 0;   // samples queued ahead of inputReadPos_

    // ── Output OLA buffer ────────────────────────────────────────────────────
    std::vector<float> outputBuffer_;
    int outputWritePos_ = 0;  // where the next synthesis frame is OLA'd in
    int outputReadPos_  = 0;  // where output samples are read from
    float synthHopAccum_= 0.0f; // fractional synthesis-hop accumulator

    // ── Phase vocoder state ──────────────────────────────────────────────────
    std::vector<float> lastPhase_;     // previous analysis frame phases (per bin)
    std::vector<float> sumPhase_;      // running synthesis phase accumulator (per bin)

    // ── Pitch resampler state ────────────────────────────────────────────────
    float resampleFrac_ = 0.0f;  // fractional position within the OLA output read

    // ── WSOLA state ──────────────────────────────────────────────────────────
    // Waveform similarity OLA for time-only (no pitch/formant) processing.
    // Uses the same input/output ring buffers as the PV path but avoids
    // FFT processing and phase accumulation.  After OLA a waveform similarity
    // search (cross-correlation over ±kWsolaSearchLen samples) finds the best
    // analysis frame alignment before each OLA step.
    //
    // kWsolaSearchLen: half-width of the waveform similarity search in samples.
    //   Larger → more accurate alignment at higher CPU cost.
    //   256 samples (≈5.8 ms at 44.1 kHz) is sufficient to absorb pitch-period
    //   jitter for F0 down to ~100 Hz (T0≈441 samples = just over 1 period).
    static constexpr int kWsolaSearchLen = 256;

    // Previous synthesis-output samples used as the reference waveform for the
    // next WSOLA similarity search.  Length = kFFTSize (the window we compare
    // against; we actually only use the overlap region = kFFTSize - kHopSize).
    std::vector<float> wsolaRef_;

    // Ideal input-read pointer for WSOLA (before applying similarity offset).
    // Maintained separately from inputReadPos_ (which both paths share) so
    // that switching from PV to WSOLA mid-stream doesn't corrupt the state.
    // Accumulated as a float to handle non-integer synthesis hops exactly.
    float wsolaInputPos_ = 0.0f;

    // ── Content-adaptive routing flag ────────────────────────────────────────
    // Set by VariphraseEngine::setAnalysis() via setForceWSOLA() when the
    // offline encode pass classifies content as ENSEMBLE or BACKING.
    // When true, processMono() routes time-only cases to processWSOLA()
    // instead of the phase vocoder inner loop.
    bool forceWSOLA_ = false;

    // ── Analysis window ──────────────────────────────────────────────────────
    std::vector<float> window_;

    // ── Public routing control ────────────────────────────────────────────────
public:
    // Called by VariphraseEngine after the offline encode pass to route
    // ENSEMBLE/BACKING content to WSOLA for time-only processing.
    void setForceWSOLA(bool v) { forceWSOLA_ = v; }

private:
    // ── Internal methods ─────────────────────────────────────────────────────

    // Windowed FFT of 'frame' (kFFTSize samples) → complex spectrum
    void analyzeFrame(const float* frame,
                      std::vector<std::complex<float>>& spectrum);

    // Cepstral-liftered spectral envelope from half-spectrum magnitudes
    void extractSpectralEnvelope(const std::vector<float>& magnitude,
                                 std::vector<float>& envelope,
                                 int lifterLength = 80);

    // In-place formant shift via spectral envelope swap
    void shiftFormants(std::vector<std::complex<float>>& spectrum,
                       const std::vector<float>& originalEnvelope,
                       float semitones);

    // Phase vocoder synthesis frame → time-domain frame (kFFTSize samples)
    // stretchRatio = totalStretch (timeStretch * pitchRatio)
    // lockToAnalysis: if true, synthesis phases are reset to the current analysis
    //   phases rather than being accumulated.  Used on transient onset frames to
    //   eliminate OLA pre-ringing and preserve attack timing.
    void synthesizeFrame(const std::vector<std::complex<float>>& spectrum,
                         std::vector<float>& frame,
                         float stretchRatio,
                         bool lockToAnalysis = false);

    // Time-domain WSOLA time-stretch — processes all available analysis frames
    // and writes to outputBuffer_.  Called from processMono when pitch and
    // formant shifts are both zero (time-only mode).
    void processWSOLA(float timeStretch);

    // In-place Cooley-Tukey FFT
    void fft(std::vector<std::complex<float>>& data, bool inverse);
};

} // namespace VSE
