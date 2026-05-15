#pragma once

#include "VariphraseEngine.h"
#include <vector>
#include <complex>

namespace VSE {

/**
 * PhaseVocoder — baseline VariPhrase algorithm implementation.
 *
 * Implements:
 *   - Time stretching via phase vocoder (OLA + phase correction)
 *   - Pitch shifting via resampling after time stretch
 *   - Formant preservation via spectral envelope scaling (cepstral liftering)
 *
 * This is the Phase 1 "baseline" implementation. Batch test results will
 * reveal its failure modes, guiding the move to SMS or LPC models.
 *
 * References:
 *   - Laroche & Dolson (1999), "Improved Phase Vocoder Time-Scale Modification"
 *   - Bonada (2000), "Automatic technique in frequency domain for near-lossless
 *     time-scale modification of audio"
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
    static constexpr int kFFTSize    = 2048;  // analysis window
    static constexpr int kHopSize    = 512;   // synthesis hop
    static constexpr int kOverlap    = kFFTSize / kHopSize;  // 4x overlap

    // ── State ────────────────────────────────────────────────────────────────
    double sampleRate_ = 44100.0;
    VariphraseParams params_ {};

    // Analysis/synthesis buffers
    std::vector<float> inputBuffer_;   // circular input accumulator
    std::vector<float> outputBuffer_;  // OLA output accumulator
    int                inputWritePos_  = 0;
    int                outputReadPos_  = 0;

    // Phase accumulation per bin
    std::vector<float> lastPhase_;     // analysis: last FFT frame phases
    std::vector<float> sumPhase_;      // synthesis: running phase sum

    // Spectral envelope (for formant preservation)
    std::vector<float> spectralEnvelope_;

    // Resampling buffer (for pitch shift after time stretch)
    std::vector<float> resampleBuffer_;
    double             resamplePhase_ = 0.0;

    // ── Internal methods ─────────────────────────────────────────────────────
    void analyzeFrame(const std::vector<float>& frame,
                      std::vector<std::complex<float>>& spectrum);

    void extractSpectralEnvelope(const std::vector<float>& magnitude,
                                 std::vector<float>& envelope,
                                 int cepstrumLifter = 80);

    void shiftFormants(std::vector<std::complex<float>>& spectrum,
                       const std::vector<float>& originalEnvelope,
                       float formantShiftSemitones);

    void synthesizeFrame(const std::vector<std::complex<float>>& spectrum,
                         std::vector<float>& frame,
                         float stretchRatio);

    void resample(const std::vector<float>& input,
                  std::vector<float>& output,
                  float ratio);

    // Hann window
    std::vector<float> window_;

    // FFT (simple DFT for now — replace with FFTW or JUCE::dsp::FFT in production)
    void fft(std::vector<std::complex<float>>& data, bool inverse);
};

} // namespace VSE
