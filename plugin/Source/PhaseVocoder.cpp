#include "PhaseVocoder.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>

namespace VSE {

static constexpr float kTwoPi = 6.28318530718f;

// Input ring buffer: kFFTSize * 4 — holds frames comfortably between flushes.
// Output OLA buffer: kFFTSize * 32 — handles up to 4× time stretch + latency.
static constexpr int kInBufSize  = 2048 * 4;   // = kFFTSize * 4
static constexpr int kOutBufSize = 2048 * 32;  // = kFFTSize * 32

// ─── FFT (Cooley-Tukey iterative, in-place) ───────────────────────────────────

void PhaseVocoder::fft(std::vector<std::complex<float>>& data, bool inverse) {
    const int N = static_cast<int>(data.size());
    if (N <= 1) return;

    // Bit-reversal permutation
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }

    // DIT butterfly
    for (int len = 2; len <= N; len <<= 1) {
        float ang = kTwoPi / float(len) * (inverse ? -1.0f : 1.0f);
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                auto u = data[i + j];
                auto v = data[i + j + len / 2] * w;
                data[i + j]           = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse)
        for (auto& x : data)
            x /= float(N);
}

// ─── Construction / Preparation ──────────────────────────────────────────────

PhaseVocoder::PhaseVocoder() {
    window_.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        window_[i] = 0.5f * (1.0f - std::cos(kTwoPi * float(i) / float(kFFTSize - 1)));
}

PhaseVocoder::~PhaseVocoder() = default;

void PhaseVocoder::prepare(double sr, int /*maxBlockSize*/) {
    sampleRate_ = sr;

    inputBuffer_.assign(kInBufSize,  0.0f);
    outputBuffer_.assign(kOutBufSize, 0.0f);

    const int halfN = kFFTSize / 2 + 1;
    lastPhase_.assign(halfN, 0.0f);
    sumPhase_.assign(halfN,  0.0f);

    wsolaRef_.assign(kFFTSize, 0.0f);

    reset();
}

void PhaseVocoder::reset() {
    std::fill(inputBuffer_.begin(),  inputBuffer_.end(),  0.0f);
    std::fill(outputBuffer_.begin(), outputBuffer_.end(), 0.0f);
    std::fill(lastPhase_.begin(),    lastPhase_.end(),    0.0f);
    std::fill(sumPhase_.begin(),     sumPhase_.end(),     0.0f);

    inputWritePos_  = 0;
    inputReadPos_   = 0;
    inputFill_      = 0;
    outputWritePos_ = 0;
    outputReadPos_  = 0;
    synthHopAccum_  = 0.0f;
    resampleFrac_   = 0.0f;

    std::fill(wsolaRef_.begin(), wsolaRef_.end(), 0.0f);
    wsolaInputPos_ = 0.0f;
}

void PhaseVocoder::setParams(const VariphraseParams& params) {
    params_ = params;
}

int PhaseVocoder::getLatencySamples() const {
    return kFFTSize;   // one analysis window of look-ahead latency
}

// ─── Analysis ────────────────────────────────────────────────────────────────

void PhaseVocoder::analyzeFrame(const float* frame,
                                 std::vector<std::complex<float>>& spectrum) {
    spectrum.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        spectrum[i] = { frame[i] * window_[i], 0.0f };
    fft(spectrum, false);
}

// ─── Spectral Envelope (cepstral liftering) ───────────────────────────────────

void PhaseVocoder::extractSpectralEnvelope(const std::vector<float>& magnitude,
                                            std::vector<float>& envelope,
                                            int lifterLength) {
    const int halfN = static_cast<int>(magnitude.size());
    const int fullN = (halfN - 1) * 2;

    // Build symmetric log-magnitude spectrum
    std::vector<std::complex<float>> logSpec(fullN, { 0.0f, 0.0f });
    for (int i = 0; i < halfN; ++i) {
        float lm = std::log(magnitude[i] + 1e-10f);
        logSpec[i] = { lm, 0.0f };
        if (i > 0 && i < halfN - 1)
            logSpec[fullN - i] = { lm, 0.0f };
    }

    // IFFT → cepstrum
    fft(logSpec, true);

    // Lifter: zero quefrencies above lifterLength (removes pitch harmonics)
    for (int i = lifterLength; i <= fullN - lifterLength; ++i)
        logSpec[i] = { 0.0f, 0.0f };

    // FFT back → log spectral envelope
    fft(logSpec, false);

    envelope.resize(halfN);
    for (int i = 0; i < halfN; ++i)
        envelope[i] = std::exp(logSpec[i].real());
}

// ─── Formant Shift ────────────────────────────────────────────────────────────

void PhaseVocoder::shiftFormants(std::vector<std::complex<float>>& spectrum,
                                  const std::vector<float>& originalEnvelope,
                                  float semitones) {
    if (std::abs(semitones) < 0.001f) return;

    const int N      = static_cast<int>(spectrum.size());
    const int halfN  = N / 2 + 1;
    const float ratio= std::pow(2.0f, semitones / 12.0f);

    std::vector<float> shiftedEnvelope(halfN, 1e-10f);
    for (int i = 0; i < halfN; ++i) {
        float srcBin = float(i) / ratio;
        int   s0     = static_cast<int>(srcBin);
        float frac   = srcBin - float(s0);
        if (s0 >= 0 && s0 < halfN - 1) {
            shiftedEnvelope[i] = originalEnvelope[s0] * (1.0f - frac)
                               + originalEnvelope[s0 + 1] * frac;
        } else {
            // srcBin is out of range (common for large downshifts where ratio < 1).
            // Hold the last valid envelope value rather than zeroing — zeroing kills
            // half the spectrum for a -12st shift and scores worse than passthrough.
            // These bins won't be frequency-shifted but at least they won't be erased.
            shiftedEnvelope[i] = originalEnvelope[halfN - 1];
        }
    }

    // Divide out original envelope, multiply in shifted one
    for (int i = 0; i < halfN; ++i) {
        float scale = shiftedEnvelope[i] / (originalEnvelope[i] + 1e-10f);
        spectrum[i] *= scale;
    }

    // Restore conjugate symmetry
    for (int i = 1; i < halfN - 1; ++i)
        spectrum[N - i] = std::conj(spectrum[i]);
}

// ─── Phase Vocoder Synthesis ──────────────────────────────────────────────────
//
// Uses the "true frequency" phase update rule (Laroche & Dolson 1999).
// stretchRatio = totalStretch = timeStretch * pitchRatio.
// Returns kFFTSize time-domain samples (NOT windowed — analysis window only).

void PhaseVocoder::synthesizeFrame(const std::vector<std::complex<float>>& spectrum,
                                    std::vector<float>& frame,
                                    float stretchRatio,
                                    bool /*lockToAnalysis*/) {
    const int halfN    = kFFTSize / 2 + 1;
    const float hopExp = kTwoPi * float(kHopSize) / float(kFFTSize);

    std::vector<std::complex<float>> synthSpec(kFFTSize, { 0.0f, 0.0f });

    for (int k = 0; k < halfN; ++k) {
        const float mag   = std::abs(spectrum[k]);
        const float phase = std::arg(spectrum[k]);

        // Normal phase accumulation (Laroche & Dolson 1999 true-frequency rule).
        // Expected phase advance for this bin over one analysis hop
        const float expectedAdv = hopExp * float(k);

        // Phase difference between this frame and last (deviation from expected)
        float delta = phase - lastPhase_[k] - expectedAdv;

        // Wrap to (−π, π]
        delta -= kTwoPi * std::round(delta / kTwoPi);

        // True instantaneous frequency
        const float trueFreq = expectedAdv + delta;

        // Accumulate synthesis phase (scaled by stretch ratio to advance the
        // synthesis hop by stretchRatio * kHopSize samples)
        sumPhase_[k] += trueFreq * stretchRatio;
        lastPhase_[k]  = phase;

        synthSpec[k] = std::polar(mag, sumPhase_[k]);
        if (k > 0 && k < halfN - 1)
            synthSpec[kFFTSize - k] = std::conj(synthSpec[k]);
    }

    // IFFT → time domain (no synthesis window — normalization handles scaling)
    fft(synthSpec, true);

    frame.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        frame[i] = synthSpec[i].real();
}

// ─── WSOLA Time-Stretch ───────────────────────────────────────────────────────
//
// Time-domain OLA with waveform similarity search (Verhelst & Roelands 1993).
// Used for time-only cases (no pitch shift, no formant shift) to preserve
// transient shape without phase-vocoder artifacts.
//
// Algorithm per frame:
//   1. Compute ideal analysis read position: n_ideal = wsolaInputPos_
//   2. Search for the integer offset Δ ∈ [−kWsolaSearchLen, kWsolaSearchLen]
//      that maximises the normalised cross-correlation between the current
//      synthesis overlap region (wsolaRef_) and the candidate input frame.
//   3. OLA the Hann-windowed input frame at the found position into outputBuffer_.
//   4. Update wsolaRef_ with the newly OLA'd region.
//   5. Advance wsolaInputPos_ by kHopSize (analysis hop).
//   6. Advance synthesis write pointer by kHopSize * timeStretch.
//
// Normalisation: same formula as the PV path (totalStretch / 2.0) because the
// Hann-window 4× overlap OLA produces the same normalisation factor.

void PhaseVocoder::processWSOLA(float timeStretch) {

    // Guard: WSOLA needs at least one full frame + search window in the buffer.
    //
    // kFFTSize (2048) for the OLA frame itself + kWsolaSearchLen (256) for the
    // maximum forward search offset = 2304.  This is larger than the PV guard
    // (kFFTSize = 2048), which means WSOLA fires one block later than PV at
    // block size 512.
    //
    // NOTE ON TIME COMPRESSION (timeStretch < 1):
    // When timeStretch < 1 the synthesis hop is smaller than kHopSize, so the
    // OLA output write pointer advances slower than the read pointer → the
    // output buffer is drained faster than it is filled → silence.
    // processWSOLA() is therefore ONLY called for timeStretch >= 1.0 (time
    // extension / no change).  Compression falls back to the phase vocoder via
    // the caller's routing logic in processMono().
    const int needed = kFFTSize + kWsolaSearchLen;

    while (inputFill_ >= needed) {

        // ── 1. Find best alignment offset (waveform similarity search) ────────
        //
        // Reference: last synthesis output stored in wsolaRef_.
        // We compare the overlap region wsolaRef_[kHopSize .. kFFTSize-1] with
        // each candidate input window at ideal position + Δ, Δ ∈ [-search, +search].
        // Only the overlap portion (length = kFFTSize - kHopSize = 1536 samples)
        // is cross-correlated to reduce computation.

        const int overlapLen  = kFFTSize - kHopSize;       // 1536
        const int idealInput  = static_cast<int>(wsolaInputPos_) % kInBufSize;

        int   bestDelta = 0;
        float bestScore = -1e30f;

        for (int delta = -kWsolaSearchLen; delta <= kWsolaSearchLen; ++delta) {
            // Candidate analysis start = idealInput + delta (wrapped)
            float num = 0.0f, denRef = 0.0f, denCand = 0.0f;
            for (int k = 0; k < overlapLen; ++k) {
                const float refSample   = wsolaRef_[kHopSize + k];
                const int   candIdx     = (idealInput + delta + k + kInBufSize) % kInBufSize;
                const float candSample  = inputBuffer_[candIdx];
                num    += refSample * candSample;
                denRef += refSample * refSample;
                denCand+= candSample * candSample;
            }
            // Guard: both vectors near-zero → score = 0 (no preference).
            // (Avoids NaN when drum decay / silence fills both reference and candidate.)
            const float denom = std::sqrt(std::max(denRef, 1e-12f) * std::max(denCand, 1e-12f));
            const float score = num / denom;
            if (score > bestScore) {
                bestScore = score;
                bestDelta = delta;
            }
        }

        // ── 2. OLA windowed input frame at the found analysis position ────────
        const int analysisStart = (idealInput + bestDelta + kInBufSize) % kInBufSize;

        for (int i = 0; i < kFFTSize; ++i) {
            const int   inIdx  = (analysisStart + i) % kInBufSize;
            const float sample = inputBuffer_[inIdx] * window_[i];
            const int   outIdx = (outputWritePos_ + i) % kOutBufSize;
            outputBuffer_[outIdx] += sample;
        }

        // ── 3. Update WSOLA reference from OLA output ─────────────────────────
        // Copy the last (kFFTSize) samples from the synthesis output into
        // wsolaRef_ so the next search has an up-to-date reference.
        for (int i = 0; i < kFFTSize; ++i) {
            const int outIdx = (outputWritePos_ + i) % kOutBufSize;
            wsolaRef_[i] = outputBuffer_[outIdx];
        }

        // ── 4. Advance positions ──────────────────────────────────────────────
        synthHopAccum_ += float(kHopSize) * timeStretch;
        const int synthHop = static_cast<int>(synthHopAccum_);
        synthHopAccum_    -= float(synthHop);
        outputWritePos_    = (outputWritePos_ + synthHop) % kOutBufSize;

        // Advance ideal input position by one analysis hop (NOT by bestDelta +
        // kHopSize — the search offset is absorbed into the alignment only; the
        // ideal cursor tracks at the natural 1:1 analysis rate so that the overall
        // time-stretch ratio is maintained across frames).
        wsolaInputPos_ += float(kHopSize);

        // Keep the shared inputReadPos_ in sync so the input buffer is consumed
        // at the correct rate and inputFill_ stays valid.
        inputReadPos_  = (inputReadPos_ + kHopSize) % kInBufSize;
        inputFill_    -= kHopSize;
    }
}

// ─── Main Process ─────────────────────────────────────────────────────────────
//
// Streaming, block-size agnostic.
//
// Axes:
//   - Time stretch:  synthesis hop = kHopSize * timeStretch
//   - Pitch shift:   synthesis hop also scaled by pitchRatio;
//                    final output resampled by 1/pitchRatio to restore duration
//   - Formant shift: spectral envelope manipulation before synthesis
//
// Normalization (analysis window only, 4× overlap, Hann):
//   norm = 2 * kHopSize * totalStretch / kFFTSize
//   (scales with totalStretch because overlap decreases as synthesis hop grows)

void PhaseVocoder::processMono(const float* input, float* output, int numSamples) {
    const float pitchRatio   = std::pow(2.0f, params_.pitchShiftSemitones / 12.0f);
    const float totalStretch = params_.timeStretchRatio * pitchRatio;
    const float formantShift = params_.formantShiftSemitones;

    // ── 1. Write incoming samples to input ring buffer ────────────────────────
    for (int i = 0; i < numSamples; ++i) {
        inputBuffer_[inputWritePos_] = input[i];
        inputWritePos_ = (inputWritePos_ + 1) % kInBufSize;
    }
    inputFill_ += numSamples;

    // ── 2. Inner loop — WSOLA or Phase Vocoder ───────────────────────────────
    //
    // Route to WSOLA if forceWSOLA_ is set (offline encode pass determined that
    // this content is ENSEMBLE or BACKING) and there is no pitch or formant
    // shift (WSOLA is a time-domain method; spectral operations require PV).
    //
    // When forceWSOLA_ is false we always use the phase vocoder path.
    //
    // Note on buffer guard: processWSOLA requires inputFill_ >= kFFTSize +
    // kWsolaSearchLen (2304) because the forward similarity search reads up to
    // kWsolaSearchLen samples past the end of the kFFTSize-sample window.
    // At block size 512 the buffer accumulates across multiple calls before
    // WSOLA first fires (≈5 blocks = 2560 samples).  This is correct — unlike
    // the failed per-frame approaches (Sessions 10/11) the WSOLA path here is
    // a FULL REPLACEMENT for the PV loop, so inputFill_ is never drained by the
    // PV loop while WSOLA is active.
    //
    // Session 10–11 historical note: WSOLA content-adaptive routing was attempted
    // The goal was to route polyphonic/drum content to WSOLA (time-domain OLA
    // with waveform similarity search) while keeping pitched/tonal content on PV.
    //
    // Three routing approaches tried — all failed:
    //
    //   a) ACF on 512-sample INPUT BLOCK (block-level, before the while loop):
    //      For vocal_aah F0=130 Hz (T0=369 samples), biased ACF[T0]/ACF[0]
    //      = (512-369)/512 ≈ 0.28 — far below any usable threshold.  Vocal
    //      routed to WSOLA → vocal_aah_time_2x 45.7 → 31.1 (−14.6 pts).
    //
    //   b) ACF on 2048-sample ANALYSIS FRAME (per-frame, inside the while loop):
    //      Correctly identified vocal as pitched (unbiased ≈ 0.984).  However,
    //      the WSOLA buffer-guard condition `inputFill_ >= kFFTSize + kWsolaSearchLen`
    //      (2304) was NEVER met because inputFill_ reaches at most kFFTSize (2048)
    //      before the loop triggers at block size 512.  WSOLA silently fell back to
    //      PV for all frames → output byte-identical to v10b → no effect.
    //
    //   c) Per-frame 2048-sample ACF with guard removed:
    //      WSOLA now activates, but the ACF-confidence distributions OVERLAP:
    //        vocal_aah:  unbiased_max  p10=0.848, median=0.980, max=1.009
    //        chord_Cmaj: unbiased_max  p10=0.000, median=0.685, max=0.920
    //      vocal p10 (0.848) < chord max (0.920): no threshold separates them.
    //      At threshold 0.90: chord frames mostly route to WSOLA, but many vocal
    //      frames (especially attack/quiet frames) also route to WSOLA.
    //      Result: vocal_aah_time_2x 45.7 → 32.0 (−13.7), NaN in drum case.
    //
    // Root cause: WSOLA routing needs a CONTENT-CLASS decision (SOLO/BACKING/
    // ENSEMBLE/LITE from V-Synth manual) that cannot be derived reliably from a
    // per-frame ACF on a single analysis window.  The V-Synth makes this decision
    // at ENCODING TIME over the entire sample.  Future work: implement full-file
    // spectral analysis for content classification before real-time processing.
    //
    // WSOLA implementation (processWSOLA) remains in the file.  Content routes
    // through WSOLA when forceWSOLA_=true (offline encode pass set ENSEMBLE or
    // BACKING), otherwise through the PV path below.

    // WSOLA is only beneficial for time EXTENSION (timeStretch >= 1.0).
    // For compression (timeStretch < 1.0) the synthesis hop is smaller than the
    // analysis hop, so the OLA output write pointer falls behind the read pointer,
    // producing silence.  Compression cases fall through to the PV path.
    const bool useWSOLA = forceWSOLA_
                       && std::abs(params_.pitchShiftSemitones)   < 0.01f
                       && std::abs(params_.formantShiftSemitones) < 0.001f
                       && params_.timeStretchRatio >= 1.0f;

    if (useWSOLA) {
        processWSOLA(params_.timeStretchRatio);
    } else {

    const int halfN = kFFTSize / 2 + 1;

    while (inputFill_ >= kFFTSize) {

        // Extract one analysis frame (kFFTSize=2048 samples) from inputReadPos_
        std::vector<float> frameData(kFFTSize);
        {
            int rp = inputReadPos_;
            for (int i = 0; i < kFFTSize; ++i) {
                frameData[i] = inputBuffer_[rp];
                rp = (rp + 1) % kInBufSize;
            }
        }

        // ── Phase vocoder path (all content) ─────────────────────────────────

        // Windowed FFT
        std::vector<std::complex<float>> spectrum;
        analyzeFrame(frameData.data(), spectrum);

        // Spectral envelope for formant manipulation
        std::vector<float> magnitude(halfN);
        for (int i = 0; i < halfN; ++i)
            magnitude[i] = std::abs(spectrum[i]);

        std::vector<float> envelope;
        extractSpectralEnvelope(magnitude, envelope);

        // Formant shift (preserves original envelope shape at new frequency)
        if (std::abs(formantShift) > 0.001f)
            shiftFormants(spectrum, envelope, formantShift);

        // Phase vocoder synthesis → time domain frame
        //
        // Session 10 note: transient phase-reset was tried here (lock sumPhase_
        // to analysis phase on onset frames with energy > 4× previous frame).
        // Result: drum_hit_time_2x improved +0.4 pts but vocal_aah_time_2x
        // regressed −9.0 pts.  The phase discontinuity broke OLA reconstruction.
        // Reverted.  True transient-synchronous behaviour requires WSOLA + event
        // stamps (V-Synth BACKING mode), not phase-vocoder phase locking.
        std::vector<float> synthFrame;
        synthesizeFrame(spectrum, synthFrame, totalStretch);

        // OLA: add synthesis frame into output buffer at outputWritePos_
        for (int i = 0; i < kFFTSize; ++i) {
            int pos = (outputWritePos_ + i) % kOutBufSize;
            outputBuffer_[pos] += synthFrame[i];
        }

        // Advance synthesis write position by synthesis hop (= kHopSize * totalStretch)
        synthHopAccum_ += float(kHopSize) * totalStretch;
        const int synthHop = static_cast<int>(synthHopAccum_);
        synthHopAccum_    -= float(synthHop);
        outputWritePos_    = (outputWritePos_ + synthHop) % kOutBufSize;

        // Advance analysis read position by one analysis hop
        inputReadPos_ = (inputReadPos_ + kHopSize) % kInBufSize;
        inputFill_   -= kHopSize;
    }

    } // end else (PV path)

    // ── 3. Read numSamples from OLA output through pitch resampler ────────────
    //
    // Normalization: analysis-window-only OLA with 4× overlap gives a sum of
    //   Σ w[n - k·Ha] ≈ 2.0  (steady state, Hann window, kOverlap=4)
    // When synthesis hop = kHopSize * totalStretch the effective overlap is
    //   kOverlap / totalStretch, so the OLA sum scales to 2/totalStretch.
    // → normFactor = totalStretch / 2.0
    //
    // Pitch resampler: reads OLA buffer at pitchRatio samples per output sample
    // (pitchRatio > 1 = pitch up = faster read = consumes more OLA samples).

    const float normFactor = totalStretch / 2.0f;
    const bool  doPitch    = std::abs(params_.pitchShiftSemitones) > 0.01f;

    for (int i = 0; i < numSamples; ++i) {
        // Linear interpolation between integer positions in the OLA buffer
        const int   ip0 = outputReadPos_;
        const int   ip1 = (outputReadPos_ + 1) % kOutBufSize;
        const float s0  = outputBuffer_[ip0];
        const float s1  = outputBuffer_[ip1];

        output[i] = normFactor * (s0 + resampleFrac_ * (s1 - s0));

        // Advance fractional read position by pitchRatio (or 1.0 if no pitch shift)
        const float advance = doPitch ? pitchRatio : 1.0f;
        resampleFrac_ += advance;

        // Consume any whole samples from the OLA buffer
        const int whole = static_cast<int>(resampleFrac_);
        resampleFrac_ -= float(whole);

        for (int k = 0; k < whole; ++k) {
            outputBuffer_[outputReadPos_] = 0.0f;
            outputReadPos_ = (outputReadPos_ + 1) % kOutBufSize;
        }
    }
}

} // namespace VSE
