#include "VariphraseEngine.h"
#include "PhaseVocoder.h"
#include "SourceFilterModel.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <atomic>
#include <mutex>

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

    void processMono(const float* input, float* output, int numSamples) {
        const Algorithm algo = algorithm.load(std::memory_order_relaxed);

        switch (algo) {
            case Algorithm::Passthrough:
                std::copy(input, input + numSamples, output);
                break;

            case Algorithm::PhaseVocoder:
                phaseVocoder->setParams(params);
                phaseVocoder->processMono(input, output, numSamples);
                break;

            case Algorithm::SinusoidalPlusResidual:
                // TODO: Implement SMS model in Phase 3
                // For now fall through to phase vocoder
                phaseVocoder->setParams(params);
                phaseVocoder->processMono(input, output, numSamples);
                break;

            case Algorithm::LPCSourceFilter:
                sourceFilter->setParams(params);
                sourceFilter->processMono(input, output, numSamples);
                break;

            case Algorithm::Hybrid: {
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
                    const bool isEnsembleOrBacking =
                        (analysis.contentType == VariphraseAnalysis::ContentType::ENSEMBLE ||
                         analysis.contentType == VariphraseAnalysis::ContentType::BACKING);
                    // WSOLA only applies to pure time-stretch (no pitch shift).
                    // For pitch-only cases on ENSEMBLE content we keep PV because
                    // WSOLA is a time-domain method and cannot shift pitch.
                    const bool wsolaCandidate = isEnsembleOrBacking && !hasPitch;
                    phaseVocoder->setForceWSOLA(wsolaCandidate);
                    phaseVocoder->setParams(params);
                    phaseVocoder->processMono(input, output, numSamples);
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

std::vector<float> VariphraseEngine::processOffline(const std::vector<float>& inputMono) {
    // Process in maxBlockSize chunks
    const int blockSize = pImpl->maxBlockSize;
    const int n = static_cast<int>(inputMono.size());
    std::vector<float> output(n + pImpl->latencySamples, 0.0f);

    for (int i = 0; i < n; i += blockSize) {
        int count = std::min(blockSize, n - i);
        const float* inPtr  = inputMono.data() + i;
        float*       outPtr = output.data() + i;

        // Pad if last block is short
        std::vector<float> inBuf(blockSize, 0.0f);
        std::copy(inPtr, inPtr + count, inBuf.data());

        std::vector<float> outBuf(blockSize, 0.0f);
        const float* inPtrs[1]  = { inBuf.data() };
        float*       outPtrs[1] = { outBuf.data() };
        process(inPtrs, outPtrs, 1, blockSize);

        std::copy(outBuf.begin(), outBuf.begin() + count, outPtr);
    }

    // Strip latency compensation
    if (pImpl->latencySamples > 0 && static_cast<int>(output.size()) > pImpl->latencySamples) {
        output.erase(output.begin(), output.begin() + pImpl->latencySamples);
    }

    output.resize(n);
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
        result.contentType = (result.peakToMeanEnergy > 5.0f)
            ? VariphraseAnalysis::ContentType::BACKING
            : VariphraseAnalysis::ContentType::ENSEMBLE;
    }

    return result;
}

void VariphraseEngine::setAnalysis(const VariphraseAnalysis& analysis) {
    pImpl->analysis = analysis;
}

} // namespace VSE
