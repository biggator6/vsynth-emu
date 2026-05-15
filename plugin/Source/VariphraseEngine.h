#pragma once

#include <vector>
#include <array>
#include <complex>
#include <memory>
#include <functional>

// NOTE: VariphraseEngine has NO JUCE dependencies by design.
// It is a pure C++ DSP class so it can be:
//   - Unit tested without a VST host
//   - Called from Python via pybind11
//   - Swapped for a different implementation without touching plugin code

namespace VSE {

// ─── Parameters ──────────────────────────────────────────────────────────────

struct VariphraseParams {
    float pitchShiftSemitones  = 0.0f;   // -24 to +24
    float timeStretchRatio     = 1.0f;   // 0.25 to 4.0
    float formantShiftSemitones= 0.0f;   // -12 to +12
    bool  robotMode            = false;  // forced monophonic voiced analysis
};

// ─── Algorithm Selection ─────────────────────────────────────────────────────

enum class Algorithm {
    Passthrough,         // No processing — for establishing null-test baseline
    PhaseVocoder,        // Phase vocoder with formant preservation (v1 baseline)
    SinusoidalPlusResidual, // SMS model (planned v2)
    LPCSourceFilter,     // LPC-based source/filter separation (planned v3)
    Hybrid,              // Best of the above (planned v4)
};

// ─── VariphraseEngine ─────────────────────────────────────────────────────────

class VariphraseEngine {
public:
    VariphraseEngine();
    ~VariphraseEngine();

    // Call once before processing starts
    void prepare(double sampleRate, int maxBlockSize);

    // Reset all internal state (call on transport stop/start)
    void reset();

    // Process a block of audio in-place.
    // input and output may be the same buffer (in-place processing supported).
    // numChannels: 1 (mono) or 2 (stereo)
    void process(const float* const* input,
                 float* const* output,
                 int numChannels,
                 int numSamples);

    // Parameter setters — thread-safe (atomic or message-queue based in real impl)
    void setParams(const VariphraseParams& params);
    VariphraseParams getParams() const;

    void setAlgorithm(Algorithm algo);
    Algorithm getAlgorithm() const;

    // ─── Offline rendering helper (used by batch_test.py pipeline) ───────────
    // Processes an entire audio buffer at once.
    // inputMono: interleaved or mono float samples
    // Returns processed output as a vector of float samples.
    std::vector<float> processOffline(const std::vector<float>& inputMono);

    // ─── Latency reporting (required for JUCE) ───────────────────────────────
    int getLatencySamples() const;

    // ─── Debug / analysis hooks ──────────────────────────────────────────────
    // Optional callback invoked each frame with internal state, for Python analysis
    using DebugCallback = std::function<void(
        const std::vector<float>& spectrum,   // magnitude spectrum (FFT output)
        const std::vector<float>& formants,   // detected formant frequencies
        float f0                              // detected fundamental frequency (0 if unvoiced)
    )>;
    void setDebugCallback(DebugCallback cb);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;  // PIMPL to keep PhaseVocoder.h out of this header
};

} // namespace VSE
