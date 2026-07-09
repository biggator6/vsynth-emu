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
    bool  polyphonicContent    = false;  // encode-pass said ENSEMBLE/BACKING;
                                         // set by Hybrid routing, read by
                                         // SourceFilterModel output-domain logic
};

// ─── Offline Content Analysis ─────────────────────────────────────────────────
//
// Mirrors the V-Synth's "encode" step: the V-Synth analyzes a loaded sample
// and assigns it an encode type (SOLO / BACKING / ENSEMBLE / LITE) plus
// extracts feature data (LPC frames, event stamps) before real-time playback.
//
// Our equivalent:
//   1. Call VariphraseEngine::analyzeContent() once on the full input buffer.
//   2. Call VariphraseEngine::setAnalysis() to store the result.
//   3. Process blocks normally — routing uses the pre-computed ContentType.
//
// This avoids per-frame routing instability: the ACF confidence of e.g.
// vocal_aah varies from 0.10 (attack) to 1.0 (steady state), making real-time
// thresholding unreliable.  A global median over all frames is stable.

struct VariphraseAnalysis {
    // V-Synth encode type (manual §3-6):
    //   LITE     — pure tones / single-oscillator content  → PV
    //   SOLO     — voiced speech / monophonic melody       → LPC source-filter
    //   ENSEMBLE — polyphonic / chords                     → WSOLA
    //   BACKING  — drums / transient-rich content          → WSOLA + event stamps
    enum class ContentType { LITE, SOLO, ENSEMBLE, BACKING };

    ContentType contentType  = ContentType::LITE;
    float medianPitchConf    = 0.0f;  // median unbiased ACF confidence (0..1+)
    float peakToMeanEnergy   = 0.0f;  // peak RMS block / mean RMS block (transient ratio)

    // Event stamps for BACKING content: sample positions of detected onsets.
    // Used to synchronize WSOLA frames to transient timing.
    std::vector<int> onsetSamples;
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

    // Pitch-synchronous granular resynthesis per Roland US6421642B1 — the
    // actual VariPhrase algorithm (see research/PATENTS.md).  Used by
    // processOffline for SOLO/LITE content; returns empty on degenerate
    // input (caller falls back to the streaming path).
    std::vector<float> granularResynthOffline(const std::vector<float>& in) const;

    // Subband time stretch per Roland US6564187B1 (simplified): quarter-octave
    // analytic sub-bands, amplitude/inst-frequency trajectories resampled to
    // the stretched timeline.  For ENSEMBLE time-only operations.
    std::vector<float> subbandStretchOffline(const std::vector<float>& in) const;

    // ─── Encode / Analysis pass ───────────────────────────────────────────────
    // analyzeContent: offline analysis of a full mono audio buffer.
    //   Computes median ACF pitch confidence over all analysis frames,
    //   classifies content type (LITE/SOLO/ENSEMBLE/BACKING), and detects
    //   onset events for BACKING content.
    //   sampleRate: Hz (used for band-energy voiced-speech detector)
    //   Call this ONCE on the full input before block-by-block processing.
    static VariphraseAnalysis analyzeContent(const float* mono,
                                              int numSamples,
                                              double sampleRate);

    // setAnalysis: store the result of analyzeContent so that processBlock
    //   can use it for stable content-adaptive routing.
    //   Thread-safe: stored with relaxed atomic, effective from next process call.
    void setAnalysis(const VariphraseAnalysis& analysis);

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
