#include "PhaseVocoder.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>

namespace VSE {

static constexpr float kTwoPi = 6.28318530718f;

// ─── FFT (Cooley-Tukey, in-place) ────────────────────────────────────────────
// NOTE: For production, replace this with juce::dsp::FFT or FFTW — this is
// a simple reference implementation sufficient for testing.

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

    // Cooley-Tukey iterative FFT
    for (int len = 2; len <= N; len <<= 1) {
        float ang = kTwoPi / static_cast<float>(len) * (inverse ? -1.0f : 1.0f);
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; ++j) {
                auto u = data[i + j];
                auto v = data[i + j + len/2] * w;
                data[i + j]           = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) {
        for (auto& x : data)
            x /= static_cast<float>(N);
    }
}

// ─── Construction ─────────────────────────────────────────────────────────────

PhaseVocoder::PhaseVocoder() {
    // Build Hann window
    window_.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        window_[i] = 0.5f * (1.0f - std::cos(kTwoPi * i / static_cast<float>(kFFTSize - 1)));
}

PhaseVocoder::~PhaseVocoder() = default;

void PhaseVocoder::prepare(double sr, int /*maxBlockSize*/) {
    sampleRate_ = sr;

    inputBuffer_.assign(kFFTSize * 4, 0.0f);
    outputBuffer_.assign(kFFTSize * 8, 0.0f);
    lastPhase_.assign(kFFTSize / 2 + 1, 0.0f);
    sumPhase_.assign(kFFTSize / 2 + 1, 0.0f);
    spectralEnvelope_.assign(kFFTSize / 2 + 1, 1.0f);
    resampleBuffer_.clear();

    reset();
}

void PhaseVocoder::reset() {
    std::fill(inputBuffer_.begin(),   inputBuffer_.end(),   0.0f);
    std::fill(outputBuffer_.begin(),  outputBuffer_.end(),  0.0f);
    std::fill(lastPhase_.begin(),     lastPhase_.end(),     0.0f);
    std::fill(sumPhase_.begin(),      sumPhase_.end(),      0.0f);
    inputWritePos_  = 0;
    outputReadPos_  = 0;
    resamplePhase_  = 0.0;
}

void PhaseVocoder::setParams(const VariphraseParams& params) {
    params_ = params;
}

int PhaseVocoder::getLatencySamples() const {
    return kFFTSize;  // one full window of latency
}

// ─── Core Processing ──────────────────────────────────────────────────────────

void PhaseVocoder::analyzeFrame(const std::vector<float>& frame,
                                 std::vector<std::complex<float>>& spectrum) {
    assert(static_cast<int>(frame.size()) == kFFTSize);
    spectrum.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        spectrum[i] = { frame[i] * window_[i], 0.0f };
    fft(spectrum, false);
}

void PhaseVocoder::extractSpectralEnvelope(const std::vector<float>& magnitude,
                                             std::vector<float>& envelope,
                                             int lifterLength) {
    // Cepstral liftering approach:
    // 1. Log magnitude spectrum
    // 2. IFFT → cepstrum
    // 3. Zero out high-quefrency terms (pitch info)
    // 4. FFT back → log spectral envelope
    // 5. Exp → magnitude envelope

    const int N = static_cast<int>(magnitude.size());
    const int fullN = (N - 1) * 2;

    // Build full complex log spectrum (symmetric)
    std::vector<std::complex<float>> logSpec(fullN);
    for (int i = 0; i < N; ++i) {
        float logMag = std::log(magnitude[i] + 1e-10f);
        logSpec[i] = { logMag, 0.0f };
        if (i > 0 && i < N - 1)
            logSpec[fullN - i] = { logMag, 0.0f };
    }

    // IFFT → cepstrum
    fft(logSpec, true);

    // Lifter: keep only low quefrency (spectral envelope), zero the rest
    std::vector<std::complex<float>> cepstrum = logSpec;
    for (int i = lifterLength; i <= fullN - lifterLength; ++i)
        cepstrum[i] = { 0.0f, 0.0f };

    // FFT back → log envelope
    fft(cepstrum, false);

    envelope.resize(N);
    for (int i = 0; i < N; ++i)
        envelope[i] = std::exp(cepstrum[i].real());
}

void PhaseVocoder::shiftFormants(std::vector<std::complex<float>>& spectrum,
                                  const std::vector<float>& originalEnvelope,
                                  float formantShiftSemitones) {
    if (std::abs(formantShiftSemitones) < 0.001f) return;

    const int N = static_cast<int>(spectrum.size());
    const int halfN = N / 2 + 1;
    const float shiftRatio = std::pow(2.0f, formantShiftSemitones / 12.0f);

    // Get magnitudes
    std::vector<float> magnitude(halfN);
    for (int i = 0; i < halfN; ++i)
        magnitude[i] = std::abs(spectrum[i]);

    // Extract target envelope (shifted)
    std::vector<float> targetEnvelope(halfN, 1.0f);
    for (int i = 0; i < halfN; ++i) {
        float srcBin = static_cast<float>(i) / shiftRatio;
        int srcIdx = static_cast<int>(srcBin);
        if (srcIdx >= 0 && srcIdx < halfN - 1) {
            float frac = srcBin - srcIdx;
            targetEnvelope[i] = originalEnvelope[srcIdx] * (1.0f - frac)
                               + originalEnvelope[srcIdx + 1] * frac;
        } else if (srcIdx >= halfN) {
            targetEnvelope[i] = 1e-10f;  // roll off above Nyquist
        }
    }

    // Apply: divide out original envelope, apply target
    for (int i = 0; i < halfN; ++i) {
        float scale = targetEnvelope[i] / (originalEnvelope[i] + 1e-10f);
        spectrum[i] *= scale;
    }

    // Maintain conjugate symmetry
    for (int i = 1; i < halfN - 1; ++i)
        spectrum[N - i] = std::conj(spectrum[i]);
}

void PhaseVocoder::synthesizeFrame(const std::vector<std::complex<float>>& spectrum,
                                    std::vector<float>& frame,
                                    float stretchRatio) {
    const int halfN = kFFTSize / 2 + 1;
    const float expectedPhaseAdv = kTwoPi * static_cast<float>(kHopSize) / static_cast<float>(kFFTSize);

    std::vector<std::complex<float>> synthSpec(kFFTSize, { 0.0f, 0.0f });

    for (int k = 0; k < halfN; ++k) {
        float magnitude = std::abs(spectrum[k]);
        float phase     = std::arg(spectrum[k]);

        // Phase advance for this bin
        float expectedAdv = expectedPhaseAdv * static_cast<float>(k);
        float phaseDiff   = phase - lastPhase_[k] - expectedAdv;

        // Wrap to [-pi, pi]
        phaseDiff -= kTwoPi * std::round(phaseDiff / kTwoPi);

        // True frequency for this bin
        float trueFreq = expectedPhaseAdv * static_cast<float>(k) + phaseDiff;

        // Accumulate synthesis phase (scaled by stretch ratio)
        sumPhase_[k] += trueFreq * stretchRatio;

        lastPhase_[k] = phase;

        // Synthesis bin
        synthSpec[k] = std::polar(magnitude, sumPhase_[k]);
        if (k > 0 && k < halfN - 1)
            synthSpec[kFFTSize - k] = std::conj(synthSpec[k]);
    }

    // IFFT
    fft(synthSpec, true);

    frame.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        frame[i] = synthSpec[i].real() * window_[i];
}

void PhaseVocoder::resample(const std::vector<float>& input,
                             std::vector<float>& output,
                             float ratio) {
    // Simple linear interpolation resampler
    // ratio = outputRate / inputRate  (ratio > 1 = upsample)
    const int inSize  = static_cast<int>(input.size());
    const int outSize = static_cast<int>(std::ceil(inSize * ratio));
    output.resize(outSize);

    for (int i = 0; i < outSize; ++i) {
        float srcPos = static_cast<float>(i) / ratio;
        int   srcIdx = static_cast<int>(srcPos);
        float frac   = srcPos - srcIdx;
        if (srcIdx + 1 < inSize) {
            output[i] = input[srcIdx] * (1.0f - frac) + input[srcIdx + 1] * frac;
        } else if (srcIdx < inSize) {
            output[i] = input[srcIdx];
        } else {
            output[i] = 0.0f;
        }
    }
}

// ─── Main Process ─────────────────────────────────────────────────────────────

void PhaseVocoder::processMono(const float* input, float* output, int numSamples) {
    const float timeStretch = params_.timeStretchRatio;
    const float pitchShift  = params_.pitchShiftSemitones;
    const float formantShift= params_.formantShiftSemitones;

    // Pitch shift = time stretch then resample back
    // e.g., +12 semitones: stretch by 2x, then resample 2x faster → same duration, higher pitch
    const float pitchRatio  = std::pow(2.0f, pitchShift / 12.0f);
    const float totalStretch = timeStretch * pitchRatio;  // combined time domain stretch needed

    // Write input to ring buffer
    for (int i = 0; i < numSamples; ++i) {
        inputBuffer_[inputWritePos_] = input[i];
        inputWritePos_ = (inputWritePos_ + 1) % static_cast<int>(inputBuffer_.size());
    }

    // Process available frames
    static int analysisHop = 0;
    analysisHop += numSamples;

    while (analysisHop >= kHopSize) {
        analysisHop -= kHopSize;

        // Extract frame from ring buffer
        std::vector<float> frame(kFFTSize);
        int readPos = (inputWritePos_ - kFFTSize + static_cast<int>(inputBuffer_.size()))
                      % static_cast<int>(inputBuffer_.size());
        for (int i = 0; i < kFFTSize; ++i) {
            frame[i] = inputBuffer_[readPos];
            readPos  = (readPos + 1) % static_cast<int>(inputBuffer_.size());
        }

        // Analyze
        std::vector<std::complex<float>> spectrum;
        analyzeFrame(frame, spectrum);

        // Extract spectral envelope for formant preservation
        const int halfN = kFFTSize / 2 + 1;
        std::vector<float> magnitude(halfN);
        for (int i = 0; i < halfN; ++i)
            magnitude[i] = std::abs(spectrum[i]);

        std::vector<float> envelope;
        extractSpectralEnvelope(magnitude, envelope);

        // Shift formants if needed (before time stretch)
        if (std::abs(formantShift) > 0.001f)
            shiftFormants(spectrum, envelope, formantShift);

        // Synthesize with phase vocoder (time stretch)
        std::vector<float> synthFrame;
        synthesizeFrame(spectrum, synthFrame, totalStretch);

        // OLA add to output buffer
        int writePos = outputReadPos_;
        for (int i = 0; i < kFFTSize; ++i) {
            int pos = (writePos + i) % static_cast<int>(outputBuffer_.size());
            outputBuffer_[pos] += synthFrame[i];
        }
    }

    // Read output
    float normFactor = static_cast<float>(kOverlap) / static_cast<float>(kFFTSize) * 2.0f;
    for (int i = 0; i < numSamples; ++i) {
        output[i] = outputBuffer_[outputReadPos_] * normFactor;
        outputBuffer_[outputReadPos_] = 0.0f;
        outputReadPos_ = (outputReadPos_ + 1) % static_cast<int>(outputBuffer_.size());
    }

    // If pitch shifting: resample the output to correct the pitch (undo the pitch-induced time stretch)
    if (std::abs(pitchShift) > 0.001f) {
        std::vector<float> outVec(output, output + numSamples);
        std::vector<float> resampled;
        resample(outVec, resampled, 1.0f / pitchRatio);
        // Trim or pad to numSamples
        resampled.resize(numSamples, 0.0f);
        std::copy(resampled.begin(), resampled.end(), output);
    }
}

} // namespace VSE
