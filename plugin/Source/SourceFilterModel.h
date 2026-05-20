#pragma once

#include "VariphraseEngine.h"
#include <array>
#include <vector>

namespace VSE {

/**
 * SourceFilterModel — LPC-based source/filter separation (Algorithm v3).
 *
 * Models the V-Synth's observed source-filter architecture:
 *   1. Detect F0 (voiced/unvoiced per frame via ACF + ZCR)
 *   2. Synthesise a band-limited sawtooth excitation at (pitch-shifted) F0
 *   3. Compute LPC coefficients from the input frame (vocal tract / formant filter)
 *   4. Shift LPC poles via root angle scaling (formant shift)
 *   5. Run LPC synthesis filter on the excitation
 *   6. OLA output frames (time stretch via variable synthesis hop)
 *
 * This allows truly independent control of all three VariPhrase axes:
 *   - Pitch: excitation F0 * pitchRatio
 *   - Time:  synthesis hop = kHopSize * timeStretch
 *   - Formant: pole angles * 2^(semitones/12)
 *
 * References:
 *   - Atal & Hanauer (1971), "Speech Analysis and Synthesis by Linear Prediction"
 *   - Moulines & Charpentier (1990), "Pitch-synchronous waveform processing"
 *   - Durand-Kerner (1966), "A method for solving all roots of a polynomial"
 */
class SourceFilterModel {
public:
    // Exposed for VariphraseEngine::Impl::prepare()
    static constexpr int kFrameSize = 1024;
    static constexpr int kHopSize   = 256;
    static constexpr int kLPCOrder  = 16;   // captures ~8 formant pairs; plateaus here for V-Synth material

    SourceFilterModel();
    ~SourceFilterModel();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void setParams(const VariphraseParams& params);
    void processMono(const float* input, float* output, int numSamples);

    int getLatencySamples() const;

private:
    double sampleRate_ = 44100.0;
    VariphraseParams params_ {};

    // ── Ring buffers ──────────────────────────────────────────────────────────
    std::vector<float> inputBuffer_;
    std::vector<float> outputBuffer_;
    int   inputWritePos_  = 0;
    int   inputReadPos_   = 0;
    int   inputFill_      = 0;
    int   outputWritePos_ = 0;
    int   outputReadPos_  = 0;
    float synthHopAccum_  = 0.0f;

    // ── Excitation state ──────────────────────────────────────────────────────
    float sawPhase_ = 0.0f;   // continuous sawtooth phase across frames

    // ── LPC filter state ──────────────────────────────────────────────────────
    std::vector<float> lpcCoeffs_;
    std::vector<float> filterState_;   // direct-form IIR memory (kLPCOrder taps)
    float lpcGain_ = 1.0f;

    // ── Cascade biquad synthesis (used when formant shift is active) ───────────
    //
    // Storing shifted poles as (kLPCOrder/2) 2nd-order all-pole sections avoids
    // the float32 quantization problem that afflicts the combined 16th-degree
    // polynomial (whose coefficients reach ±214).  Per-section biquad coefficients
    // are bounded: |b1| ≤ 2·|pole|, b0 = |pole|² ≤ 1.  Float32 precision is
    // therefore identical to float64 for each section, and the cascade remains
    // analytically stable even after the float32 cast.
    struct BiquadSection { float b1, b0; };   // 1/(1 + b1·z⁻¹ + b0·z⁻²)
    std::vector<BiquadSection>            biquads_;      // populated by shiftFormants
    std::vector<std::array<float, 2>>     biquadState_;  // per-section [y[n-1], y[n-2]]
    bool                                   useBiquad_ = false;

    // ── Onset detection state ─────────────────────────────────────────────────
    // Tracks previous-frame RMS energy for onset detection.
    // When input energy rises sharply (transient onset), the synthesis frame is
    // blended toward a windowed pass-through of the input frame, preserving the
    // timing and sharpness of the onset rather than smoothing it via OLA.
    float prevFrameRMS_ = 0.0f;

    // ── Analysis window ───────────────────────────────────────────────────────
    std::vector<float> window_;

    // ── DSP methods ───────────────────────────────────────────────────────────

    // Levinson-Durbin LPC analysis — returns coefficients and prediction gain
    void computeLPC(const std::vector<float>& frame,
                    std::vector<float>& coeffs,
                    float& gain);

    // Shift formant frequencies by semitones via LPC pole angle scaling
    void shiftFormants(std::vector<float>& coeffs, float semitones);

    // Autocorrelation-based F0 detection with parabolic interpolation
    // Returns 0 if unvoiced or F0 undetectable
    float estimateF0(const std::vector<float>& frame) const;

    // Voiced/unvoiced detection via ZCR + energy
    bool isVoiced(const std::vector<float>& frame) const;

    // Band-limited sawtooth excitation synthesis at f0_hz
    // phase is maintained across calls for phase continuity
    void synthesiseExcitation(std::vector<float>& excitation,
                               int numSamples,
                               float f0_hz,
                               float& phase);

    // All-pole LPC synthesis filter (direct form, IIR)
    void synthesisFilter(const std::vector<float>& excitation,
                          const std::vector<float>& coeffs,
                          float gain,
                          std::vector<float>& output);

    // All-pole LPC synthesis filter as cascade of 2nd-order sections (biquads_).
    // Each section is y[n] = x[n] − b1·y[n−1] − b0·y[n−2].
    // Used when formant shift is active to avoid float32 quantization of the
    // combined polynomial's large coefficients.
    void synthesisFilterBiquad(const std::vector<float>& excitation,
                                float gain,
                                std::vector<float>& output);
};

} // namespace VSE
