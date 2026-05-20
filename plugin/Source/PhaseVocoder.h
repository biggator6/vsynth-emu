#pragma once

#include "VariphraseEngine.h"
#include <vector>
#include <complex>

namespace VSE {

/**
 * PhaseVocoder — baseline VariPhrase algorithm (Algorithm v1).
 *
 * Implements independent control of:
 *   - Time stretch  — phase vocoder OLA with variable synthesis hop
 *   - Pitch shift   — stretch by pitchRatio, then streaming linear resample back
 *   - Formant shift — cepstral-liftered spectral envelope warp in frequency domain
 *
 * Ring-buffer streaming design: processMono() accepts arbitrary block sizes.
 *
 * References:
 *   - Laroche & Dolson (1999), "Improved Phase Vocoder Time-Scale Modification"
 *   - Bonada (2000), "Automatic technique in frequency domain for near-lossless TSM"
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

    // ── Analysis window ──────────────────────────────────────────────────────
    std::vector<float> window_;

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
    void synthesizeFrame(const std::vector<std::complex<float>>& spectrum,
                         std::vector<float>& frame,
                         float stretchRatio);

    // In-place Cooley-Tukey FFT
    void fft(std::vector<std::complex<float>>& data, bool inverse);
};

} // namespace VSE
