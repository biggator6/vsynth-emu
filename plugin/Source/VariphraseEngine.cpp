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

    // Valid (non-starvation-padding) prefix of the last processMono output.
    // Set per call from the active processor; numSamples for passthrough.
    int lastValidOutput = 0;

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
        lastValidOutput = numSamples;

        switch (algo) {
            case Algorithm::Passthrough:
                std::copy(input, input + numSamples, output);
                break;

            case Algorithm::PhaseVocoder:
                phaseVocoder->setParams(params);
                phaseVocoder->processMono(input, output, numSamples);
                lastValidOutput = phaseVocoder->getLastValidOutput();
                break;

            case Algorithm::SinusoidalPlusResidual:
                // TODO: Implement SMS model in Phase 3
                // For now fall through to phase vocoder
                phaseVocoder->setParams(params);
                phaseVocoder->processMono(input, output, numSamples);
                lastValidOutput = phaseVocoder->getLastValidOutput();
                break;

            case Algorithm::LPCSourceFilter:
                sourceFilter->setParams(params);
                sourceFilter->processMono(input, output, numSamples);
                lastValidOutput = sourceFilter->getLastValidOutput();
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
                    lastValidOutput = sourceFilter->getLastValidOutput();
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
                    // WSOLA routing disabled (Session 15): after the v21 PV
                    // overhaul (inverted hops, identity phase locking, FFT
                    // sign fix) the phase vocoder beats WSOLA on every case
                    // that previously routed there — drum_hit_time_halfspeed,
                    // the last WSOLA case, scores 31.1 on PV vs 26.0 on WSOLA.
                    // processWSOLA() is retained in PhaseVocoder for reference.
                    //
                    // LITE content (pure tones) routes to the LPC source-filter
                    // even for time-only operations (Session 15): the V-Synth
                    // RESYNTHESIZES — its time-stretched sine reference carries
                    // strong sawtooth harmonics (880 Hz at −18 dB rel.) that a
                    // transparent PV stretch of the pure input can never have.
                    // sine_440_time_2x: 36.6 LPC vs 15.7 PV; pitch_up7st 43.1
                    // LPC vs 26.0 PV.  EXCEPT downward pitch: the LPC envelope
                    // (resonant at the original F0) boosts the wrong harmonic
                    // of the lowered excitation — pitch_down12st 6.1 LPC vs
                    // 48.8 PV.  SOLO (vocal) and BACKING/ENSEMBLE stay on PV.
                    if (analysis.contentType == VariphraseAnalysis::ContentType::LITE &&
                        params.pitchShiftSemitones > -0.5f) {
                        sourceFilter->setParams(params);
                        sourceFilter->processMono(input, output, numSamples);
                        lastValidOutput = sourceFilter->getLastValidOutput();
                    } else {
                        phaseVocoder->setForceWSOLA(false);
                        phaseVocoder->setParams(params);
                        phaseVocoder->processMono(input, output, numSamples);
                        lastValidOutput = phaseVocoder->getLastValidOutput();
                    }
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

// ─── Pitch-Synchronous Granular Resynthesis (US6421642B1) ────────────────────
//
// The actual VariPhrase algorithm per Roland's playback patent (filed
// 2000-05-02; see research/PATENTS.md).  Encode cuts the phrase into
// ~one-pitch-period grains ("cut waveforms"), each with its measured pitch
// (cwp).  Playback:
//   TIME    — playing position pp advances by 1/timeStretch per output
//             sample; pp selects WHICH grain is current.
//   PITCH   — grains re-trigger at the target period ppw = cwp / pitchRatio,
//             on TWO channels offset by half a period, each with a
//             triangular window (offset triangles sum to a constant).
//   FORMANT — grain READ VELOCITY fsv: resampling the grain content scales
//             the spectral envelope.  Window length wl = cwp / fsv, CLAMPED
//             to ≤ ppw (the patent's clamp — the mechanism behind the
//             ~0.75× upward formant saturation measured in Session 15).

namespace {

struct GrainCut { int start; float cwp; bool voiced; };

// Local pitch-period estimate around `start` — normalized ACF with an
// integer-subharmonic octave guard (same guards as SourceFilterModel's
// estimateF0, compact form).  prevPeriod, when > 0, narrows the search ±25 %
// for speed and continuity; a low-confidence result falls back to full range.
float estimateLocalPeriod(const std::vector<float>& x, int start, double sr,
                          float prevPeriod) {
    const int n = (int)x.size();
    const int w = std::min(1024, n - start);
    if (w < 256) return 0.0f;

    int lagLo = std::max(2, (int)(sr / 500.0));
    int lagHi = std::min(w - 1, (int)(sr / 60.0));
    if (prevPeriod > 0.0f) {
        lagLo = std::max(lagLo, (int)(prevPeriod * 0.75f));
        lagHi = std::min(lagHi, (int)(prevPeriod * 1.25f));
        if (lagHi <= lagLo) { lagLo = std::max(2, (int)(sr / 500.0));
                              lagHi = std::min(w - 1, (int)(sr / 60.0)); }
    }

    double e0 = 0.0;
    for (int i = 0; i < w; ++i) e0 += double(x[start+i]) * x[start+i];
    if (e0 < 1e-9) return 0.0f;

    int bestLag = 0; double bestV = 0.0;
    for (int lag = lagLo; lag <= lagHi; ++lag) {
        double s = 0.0;
        for (int i = 0; i + lag < w; ++i)
            s += double(x[start+i]) * x[start+i+lag];
        const double v = (s / e0) * (double(w) / double(w - lag));  // unbiased
        if (v > bestV) { bestV = v; bestLag = lag; }
    }
    if (bestV < 0.3) {
        // Retry full range once before declaring unvoiced
        if (prevPeriod > 0.0f) return estimateLocalPeriod(x, start, sr, 0.0f);
        return 0.0f;
    }
    // Octave guard: prefer lag/2 if nearly as strong
    if (bestLag / 2 >= lagLo) {
        const int half = bestLag / 2;
        double s = 0.0;
        for (int i = 0; i + half < w; ++i)
            s += double(x[start+i]) * x[start+i+half];
        const double v = (s / e0) * (double(w) / double(w - half));
        if (v >= 0.95 * bestV) bestLag = half;
    }
    return float(bestLag);
}

std::vector<GrainCut> encodeGrainCuts(const std::vector<float>& x, double sr) {
    std::vector<GrainCut> cuts;
    const int n = (int)x.size();
    int pos = 0;
    float prev = 0.0f;
    while (pos + 256 < n) {
        const float p = estimateLocalPeriod(x, pos, sr, prev);
        if (p > 0.0f) {
            cuts.push_back({ pos, p, true });
            prev = p;
            pos += std::max(2, (int)std::lround(p));
        } else {
            cuts.push_back({ pos, 256.0f, false });
            prev = 0.0f;
            pos += 256;
        }
    }
    return cuts;
}

} // namespace

std::vector<float> VariphraseEngine::granularResynthOffline(
        const std::vector<float>& in) const {
    const int n = (int)in.size();
    const double sr = pImpl->sampleRate;
    const float stretch    = std::max(0.05f, pImpl->params.timeStretchRatio);
    const float pitchRatio = std::pow(2.0f, pImpl->params.pitchShiftSemitones / 12.0f);
    const float fsv        = std::pow(2.0f, pImpl->params.formantShiftSemitones / 12.0f);

    const std::vector<GrainCut> cuts = encodeGrainCuts(in, sr);
    if (cuts.size() < 2) return {};

    const int outLen = std::max(1, (int)std::lround((double)n * stretch));
    std::vector<float> out(outLen, 0.0f);

    // One active grain per channel (wl ≤ ppw guarantees no within-channel
    // overlap).  age is in output samples since trigger.
    struct Grain { double srcStart = 0, wl = 0, age = 1e18; };
    Grain ch[2];
    double nextTrig[2] = { 0.0, -1.0 };   // ch2 initialised at first ppw/2

    const double ppInc = 1.0 / stretch;   // playing-position advance per output sample
    double pp = 0.0;
    size_t cutIdx = 0;

    for (int t = 0; t < outLen; ++t, pp += ppInc) {
        // Track the current cut from the playing position
        while (cutIdx + 1 < cuts.size() && pp >= (double)cuts[cutIdx + 1].start)
            ++cutIdx;
        const GrainCut& cut = cuts[cutIdx];
        const double ppw = std::max(2.0, (double)cut.cwp / (double)pitchRatio);
        if (nextTrig[1] < 0.0) nextTrig[1] = ppw * 0.5;

        for (int c = 0; c < 2; ++c) {
            if ((double)t >= nextTrig[c]) {
                const double wlFull = (double)cut.cwp / (double)fsv;
                ch[c].wl  = std::min(wlFull, ppw);
                // Patent read offset (US6421642): when the window clamps to
                // ppw, the grain reads the LAST ppw×fsv samples of the cut
                // (os = cwp − ppw·fsv), not the first — the read region is
                // anchored to the cut END so the covered content stays
                // centred as ppw shrinks (pitch-up).
                const double os = (wlFull > ppw)
                    ? (double)cut.cwp - ppw * (double)fsv
                    : 0.0;
                ch[c].srcStart = (double)cut.start + os;
                ch[c].age = 0.0;
                nextTrig[c] += ppw;
            }
            if (ch[c].age < ch[c].wl) {
                const double srcPos = ch[c].srcStart + ch[c].age * (double)fsv;
                const int    i0     = (int)srcPos;
                if (i0 >= 0 && i0 + 1 < n) {
                    const double frac = srcPos - i0;
                    const double s    = in[i0] * (1.0 - frac) + in[i0 + 1] * frac;
                    // Triangular window 0→1→0 over wl
                    const double ph = ch[c].age / ch[c].wl;
                    const double wv = 1.0 - std::abs(2.0 * ph - 1.0);
                    out[t] += (float)(s * wv);
                }
                ch[c].age += 1.0;
            }
        }
    }
    return out;
}

std::vector<float> VariphraseEngine::processOffline(const std::vector<float>& inputMono) {
    // ── Pitch-synchronous granular for SOLO (Session 15, US6421642) ──────────
    // The patent engine handles all three axes in one mechanism.  SOLO only:
    // measured on the suite, granular lifts every vocal case (formant_upmax
    // +16.3, spectral sim 0.29-0.41 → 0.38-0.56) but drops every sine case
    // (formant_upmax −38.6) — consistent with Roland's "LITE" being the
    // reduced-analysis mode that does NOT run the full phrase engine.  LITE
    // keeps the v23 routing (LPC resynthesis / PV).
    if (pImpl->algorithm.load(std::memory_order_relaxed) == Algorithm::Hybrid &&
        pImpl->analysis.contentType == VariphraseAnalysis::ContentType::SOLO) {
        std::vector<float> out = granularResynthOffline(inputMono);
        if (!out.empty()) return out;
    }

    // ── Event-based stretch for BACKING content (Session 15) ─────────────────
    // V-Synth BACKING mode stores onset stamps at encode time and keeps the
    // transients intact under time stretch: each attack is placed VERBATIM at
    // its stretched output position; only the decay/silence between events is
    // actually time-stretched.  Time-only operations on BACKING content with
    // detected onsets take this path; everything else falls through to the
    // normal streaming render below.
    {
        const float st = pImpl->params.timeStretchRatio;
        const bool timeOnly =
            std::abs(pImpl->params.pitchShiftSemitones)   < 0.01f &&
            std::abs(pImpl->params.formantShiftSemitones) < 0.001f;
        if (pImpl->analysis.contentType == VariphraseAnalysis::ContentType::BACKING &&
            timeOnly && std::abs(st - 1.0f) > 0.01f &&
            !pImpl->analysis.onsetSamples.empty()) {

            const int n = (int)inputMono.size();
            const int outLenTotal = std::max(1, (int)std::lround((double)n * st));
            std::vector<float> out(outLenTotal, 0.0f);

            // Helper: stretch a vector to an exact target length via the PV.
            auto pvStretch = [&](const std::vector<float>& seg, int targetLen)
                    -> std::vector<float> {
                if (seg.empty() || targetLen <= 0) return {};
                VariphraseParams p {};
                p.timeStretchRatio = float(targetLen) / float(seg.size());
                pImpl->phaseVocoder->setParams(p);
                pImpl->phaseVocoder->reset();
                pImpl->phaseVocoder->setOutputCapacity(targetLen + 8192);
                pImpl->phaseVocoder->setForceWSOLA(false);

                std::vector<float> res;
                res.reserve(targetLen + 1024);
                const int bs = pImpl->maxBlockSize;
                std::vector<float> inBuf(bs), outBuf(bs);
                int fed = 0;
                const int maxIters = 4 * ((int)seg.size() + targetLen) / bs + 64;
                for (int it = 0; it < maxIters &&
                       (fed < (int)seg.size() || (int)res.size() < targetLen); ++it) {
                    std::fill(inBuf.begin(), inBuf.end(), 0.0f);
                    if (fed < (int)seg.size()) {
                        const int c = std::min(bs, (int)seg.size() - fed);
                        std::copy(seg.begin() + fed, seg.begin() + fed + c, inBuf.begin());
                        fed += c;
                    }
                    pImpl->phaseVocoder->processMono(inBuf.data(), outBuf.data(), bs);
                    const int valid = std::min(pImpl->phaseVocoder->getLastValidOutput(), bs);
                    if (valid > 0)
                        res.insert(res.end(), outBuf.begin(), outBuf.begin() + valid);
                }
                res.resize(targetLen, 0.0f);
                return res;
            };

            // Segment boundaries: [0, onset_1, onset_2, …, n]
            std::vector<int> bounds { 0 };
            for (int e : pImpl->analysis.onsetSamples)
                if (e > bounds.back() && e < n) bounds.push_back(e);
            if (bounds.back() != n) bounds.push_back(n);

            constexpr int kAttackLen = 2048;   // ≈43 ms at 48 kHz, kept verbatim
            constexpr int kXfade     = 128;

            for (int b = 0; b + 1 < (int)bounds.size(); ++b) {
                const int s = bounds[b], e = bounds[b + 1];
                const int segLen   = e - s;
                const int outStart = (int)std::lround((double)s * st);
                const int outEnd   = std::min(outLenTotal, (int)std::lround((double)e * st));
                const int outLen   = outEnd - outStart;
                if (segLen <= 0 || outLen <= 0) continue;

                // Segments that begin at a detected onset keep their attack
                // verbatim; the leading pre-onset segment has no attack.
                const bool atOnset  = (b > 0);
                const int attackLen = atOnset ? std::min({ kAttackLen, segLen, outLen })
                                              : 0;
                for (int i = 0; i < attackLen; ++i)
                    out[outStart + i] = inputMono[s + i];

                const int tailInLen  = segLen - attackLen;
                const int tailOutLen = outLen - attackLen;
                if (tailInLen > 0 && tailOutLen > 0) {
                    std::vector<float> tail(inputMono.begin() + s + attackLen,
                                            inputMono.begin() + e);
                    std::vector<float> stretched = pvStretch(tail, tailOutLen);
                    for (int i = 0; i < tailOutLen; ++i) {
                        float v = stretched[i];
                        // Short crossfade out of the verbatim attack
                        if (attackLen > 0 && i < kXfade) {
                            const float w = float(i) / float(kXfade);
                            v = w * v + (1.0f - w) * out[outStart + attackLen + i];
                        }
                        out[outStart + attackLen + i] = v;
                    }
                }
            }
            return out;
        }
    }

    // Session 14 rework: drain mode for time stretch.
    //
    // The old implementation collected exactly inputLen output samples.  Two
    // problems for timeStretch ≠ 1:
    //   1. The stretched signal is inputLen×stretch long; the tail was lost.
    //   2. For stretch > 1 the engines' OLA write pointer advances stretch×
    //      faster than the read pointer.  The surplus grows to
    //      inputLen×(stretch−1), lapping the default 64k output ring after
    //      ~1.4 s at 48 kHz and corrupting the OLA sum (confirmed: the
    //      vocal_aah_time_halfspeed render degraded at exactly 1.37 s).
    //
    // Fix: pre-size the engine output rings to the full render, feed all the
    // input, then keep pumping zero-input blocks ("drain") until the expected
    // inputLen×stretch samples (plus latency) have been collected.  Trailing
    // zero blocks are harmless: their Hann-windowed OLA contribution is zero,
    // and the queued surplus drains through the normal read path.
    const int   blockSize = pImpl->maxBlockSize;
    const int   n         = static_cast<int>(inputMono.size());
    const float stretch   = std::max(0.05f, pImpl->params.timeStretchRatio);
    const int   expected  = std::max(1, (int)std::lround((double)n * (double)stretch));

    if (pImpl->phaseVocoder) pImpl->phaseVocoder->setOutputCapacity(expected + 8 * blockSize);
    if (pImpl->sourceFilter) pImpl->sourceFilter->setOutputCapacity(expected + 8 * blockSize);

    std::vector<float> output;
    output.reserve(expected + blockSize);

    int fed = 0;
    // Safety cap: enough iterations to feed all input AND collect all output
    // (the valid prefix per call can be < blockSize, so allow generous slack).
    const int maxIters = 4 * ((n + expected) / blockSize) + 64;
    for (int iter = 0;
         (fed < n || (int)output.size() < expected) && iter < maxIters;
         ++iter) {
        std::vector<float> inBuf(blockSize, 0.0f);
        if (fed < n) {
            const int count = std::min(blockSize, n - fed);
            std::copy(inputMono.begin() + fed, inputMono.begin() + fed + count,
                      inBuf.begin());
            fed += count;
        }

        std::vector<float> outBuf(blockSize, 0.0f);
        const float* inPtrs[1]  = { inBuf.data() };
        float*       outPtrs[1] = { outBuf.data() };
        process(inPtrs, outPtrs, 1, blockSize);

        // Keep only the valid (non-starvation-padding) prefix.  Read gating in
        // the processors means engine latency zeros and compression starvation
        // gaps are reported as invalid rather than collected — so no separate
        // latency strip is needed and the stream is contiguous signal.
        const int valid = std::min(pImpl->lastValidOutput, blockSize);
        if (valid > 0)
            output.insert(output.end(), outBuf.begin(), outBuf.begin() + valid);

        // After all input is fed, stop once the engines run dry.
        if (fed >= n && valid == 0 && iter > (n / blockSize) + 8 &&
            (int)output.size() >= expected)
            break;
    }

    output.resize(expected);
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

void VariphraseEngine::setAnalysis(const VariphraseAnalysis& analysis) {
    pImpl->analysis = analysis;
}

} // namespace VSE
