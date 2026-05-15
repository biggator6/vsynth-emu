#include "SourceFilterModel.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>

namespace VSE {

SourceFilterModel::SourceFilterModel() = default;
SourceFilterModel::~SourceFilterModel() = default;

void SourceFilterModel::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate;

    inputBuffer_.assign(kFrameSize * 4, 0.0f);
    outputBuffer_.assign(kFrameSize * 4, 0.0f);
    lpcCoeffs_.assign(kLPCOrder, 0.0f);
    filterState_.assign(kLPCOrder, 0.0f);

    // Hann window
    window_.resize(kFrameSize);
    for (int i = 0; i < kFrameSize; ++i)
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * i / (kFrameSize - 1)));

    reset();
}

void SourceFilterModel::reset()
{
    std::fill(inputBuffer_.begin(),  inputBuffer_.end(),  0.0f);
    std::fill(outputBuffer_.begin(), outputBuffer_.end(), 0.0f);
    std::fill(filterState_.begin(),  filterState_.end(),  0.0f);
    inputWritePos_ = 0;
    outputReadPos_ = 0;
    lpcGain_       = 1.0f;
}

void SourceFilterModel::setParams(const VariphraseParams& params)
{
    params_ = params;
}

void SourceFilterModel::processMono(const float* input, float* output, int numSamples)
{
    // TODO: implement full source-filter processing pipeline
    // Placeholder: pass through unmodified
    std::copy(input, input + numSamples, output);
}

int SourceFilterModel::getLatencySamples() const
{
    return kFrameSize;
}

// ── LPC (Levinson-Durbin) ────────────────────────────────────────────────────

void SourceFilterModel::computeLPC(const std::vector<float>& frame,
                                   std::vector<float>& coeffs,
                                   float& gain)
{
    assert(static_cast<int>(frame.size()) == kFrameSize);
    coeffs.assign(kLPCOrder, 0.0f);

    // Autocorrelation
    std::vector<double> r(kLPCOrder + 1, 0.0);
    for (int lag = 0; lag <= kLPCOrder; ++lag)
        for (int n = 0; n < kFrameSize - lag; ++n)
            r[lag] += double(frame[n]) * double(frame[n + lag]);

    if (r[0] < 1e-10) { gain = 0.0f; return; }

    // Levinson-Durbin recursion
    std::vector<double> a(kLPCOrder + 1, 0.0);
    std::vector<double> aPrev(kLPCOrder + 1, 0.0);
    double error = r[0];

    for (int i = 1; i <= kLPCOrder; ++i)
    {
        double lambda = 0.0;
        for (int j = 1; j < i; ++j)
            lambda -= a[j] * r[i - j];
        lambda = (lambda + r[i]) / error;

        aPrev = a;
        a[i] = lambda;
        for (int j = 1; j < i; ++j)
            a[j] = aPrev[j] - lambda * aPrev[i - j];

        error *= (1.0 - lambda * lambda);
        if (error < 1e-10) break;
    }

    for (int i = 0; i < kLPCOrder; ++i)
        coeffs[i] = float(a[i + 1]);

    gain = float(std::sqrt(std::max(error, 1e-10)));
}

// ── Voiced/unvoiced ──────────────────────────────────────────────────────────

bool SourceFilterModel::isVoiced(const std::vector<float>& frame) const
{
    // Simple zero-crossing rate heuristic — voiced speech has low ZCR
    int crossings = 0;
    for (int i = 1; i < static_cast<int>(frame.size()); ++i)
        if ((frame[i] >= 0.0f) != (frame[i - 1] >= 0.0f))
            ++crossings;

    float zcr = float(crossings) / float(frame.size());
    return zcr < 0.1f;  // threshold TBD from V-Synth recordings
}

float SourceFilterModel::estimateF0(const std::vector<float>& frame) const
{
    // TODO: autocorrelation-based F0 estimation (YIN or AMDF)
    (void)frame;
    return 0.0f;
}

// ── Source manipulation ───────────────────────────────────────────────────────

void SourceFilterModel::shiftPitch(std::vector<float>& excitation, float semitones)
{
    // TODO: PSOLA or resampling on the excitation signal
    (void)excitation; (void)semitones;
}

void SourceFilterModel::stretchTime(const std::vector<float>& excitation,
                                    std::vector<float>& output,
                                    float ratio)
{
    // TODO: PSOLA-based time stretching
    (void)ratio;
    output = excitation;
}

// ── Filter (formant) manipulation ─────────────────────────────────────────────

void SourceFilterModel::shiftFormants(std::vector<float>& coeffs, float semitones)
{
    // Frequency-warp LPC coefficients to shift formants.
    // Bilinear transform with warp factor derived from semitone shift.
    // TODO: implement bilinear warping
    (void)coeffs; (void)semitones;
}

void SourceFilterModel::synthesize(const std::vector<float>& excitation,
                                   const std::vector<float>& coeffs,
                                   float gain,
                                   float* output,
                                   int numSamples)
{
    // All-pole IIR filter (LPC synthesis filter)
    assert(static_cast<int>(coeffs.size()) == kLPCOrder);
    for (int n = 0; n < numSamples && n < static_cast<int>(excitation.size()); ++n)
    {
        float y = gain * excitation[n];
        for (int k = 0; k < kLPCOrder; ++k)
            y -= coeffs[k] * filterState_[k];

        // Shift filter state
        for (int k = kLPCOrder - 1; k > 0; --k)
            filterState_[k] = filterState_[k - 1];
        filterState_[0] = y;

        output[n] = y;
    }
}

} // namespace VSE
