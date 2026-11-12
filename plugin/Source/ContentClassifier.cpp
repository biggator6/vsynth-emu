// ContentClassifier.cpp: Auto's classifier, analyzeContent, moved as it stood from the legacy VariphraseEngine.cpp
// when v191 removed that engine.  PluginEncodes.cpp's encodeWave calls it for a wave whose Encode Type is Auto.
#include "ContentClassifier.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

namespace VSE {

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

VariphraseAnalysis analyzeContent(const float* mono,
                                                     int           numSamples,
                                                     double        sampleRate, const std::atomic<bool>* stop) {
    VariphraseAnalysis result;
    if (numSamples < 2048 || !std::isfinite(sampleRate) || sampleRate <= 0.0) return result;

    const int kFrameSize = 2048;
    const int kStep      = 512;
    const int lagLo = static_cast<int>(std::max(1.0, sampleRate / 500.0));  //  ~88 @ 44.1kHz
    const int lagHi = static_cast<int>(std::min(double(kFrameSize - 1),
                                                sampleRate / 60.0));       // ~735 @ 44.1kHz

    std::vector<float> confValues;
    std::vector<float> rmsValues;

    // ── Per-frame unbiased ACF confidence (no further frame once a stop given goes up, v185) ──
    for (int start = 0; start + kFrameSize <= numSamples && !(stop && *stop); start += kStep) {
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
        const double omega = 2.0 * std::numbers::pi * fc / sampleRate;
        const double R     = 1.0 - (omega / (2.0 * Q));
        const double cosOm = std::cos(omega);
        const double bpG   = (1.0 - R * R) * 0.5;

        double bpEnergy    = 0.0;
        double totalEnergy = 0.0;
        double x_prev2 = 0.0, x_prev1 = 0.0;
        double y_prev2 = 0.0, y_prev1 = 0.0;

        const int analyzed = static_cast<int>(std::min(double(numSamples), sampleRate)); // first second
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

} // namespace VSE
