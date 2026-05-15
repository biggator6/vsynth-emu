#pragma once

#include "VariphraseEngine.h"
#include <vector>

namespace VSE {

/**
 * SourceFilterModel — LPC-based source/filter separation for VariPhrase algorithm v3.
 *
 * Separates audio into:
 *   - Source: glottal excitation (voiced) or noise (unvoiced)
 *   - Filter: vocal tract spectral envelope estimated via LPC
 *
 * Allows independent manipulation of pitch (source), duration (source rate),
 * and formants (filter coefficients) — matching VariPhrase's three decoupled axes.
 *
 * References:
 *   - Atal & Hanauer (1971), "Speech Analysis and Synthesis by Linear Prediction"
 *   - Moulines & Charpentier (1990), "Pitch-synchronous waveform processing"
 */
class SourceFilterModel {
public:
    SourceFilterModel();
    ~SourceFilterModel();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setParams(const VariphraseParams& params);
    void processMono(const float* input, float* output, int numSamples);

    int getLatencySamples() const;

private:
    static constexpr int kLPCOrder     = 16;   // LPC order (formant resolution)
    static constexpr int kFrameSize    = 1024;  // analysis frame in samples
    static constexpr int kHopSize      = 256;   // hop between frames

    double sampleRate_ = 44100.0;
    VariphraseParams params_ {};

    // ── LPC analysis ────────────────────────────────────────────────────────
    void computeLPC(const std::vector<float>& frame,
                    std::vector<float>& coeffs,
                    float& gain);

    // ── Voiced/unvoiced detection ────────────────────────────────────────────
    bool isVoiced(const std::vector<float>& frame) const;
    float estimateF0(const std::vector<float>& frame) const;  // returns 0 if unvoiced

    // ── Source manipulation ──────────────────────────────────────────────────
    void shiftPitch(std::vector<float>& excitation, float semitones);
    void stretchTime(const std::vector<float>& excitation,
                     std::vector<float>& output,
                     float ratio);

    // ── Filter (formant) manipulation ────────────────────────────────────────
    void shiftFormants(std::vector<float>& coeffs, float semitones);
    void synthesize(const std::vector<float>& excitation,
                    const std::vector<float>& coeffs,
                    float gain,
                    float* output,
                    int numSamples);

    // ── Buffers ──────────────────────────────────────────────────────────────
    std::vector<float> inputBuffer_;
    std::vector<float> outputBuffer_;
    std::vector<float> lpcCoeffs_;
    std::vector<float> filterState_;  // IIR filter memory
    int inputWritePos_  = 0;
    int outputReadPos_  = 0;
    float lpcGain_      = 1.0f;

    std::vector<float> window_;
};

} // namespace VSE
