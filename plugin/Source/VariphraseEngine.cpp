#include "VariphraseEngine.h"
#include "PhaseVocoder.h"

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
    std::unique_ptr<PhaseVocoder> phaseVocoder;

    // Latency in samples (set by prepare())
    int latencySamples = 0;

    // Debug
    VariphraseEngine::DebugCallback debugCallback;
    std::mutex debugMutex;

    void prepare(double sr, int blockSize) {
        sampleRate   = sr;
        maxBlockSize = blockSize;

        phaseVocoder = std::make_unique<PhaseVocoder>();
        phaseVocoder->prepare(sr, blockSize);

        latencySamples = phaseVocoder->getLatencySamples();
    }

    void reset() {
        if (phaseVocoder) phaseVocoder->reset();
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
                // TODO: Implement LPC source-filter in Phase 4
                phaseVocoder->setParams(params);
                phaseVocoder->processMono(input, output, numSamples);
                break;

            case Algorithm::Hybrid:
                // TODO: Implement hybrid approach in Phase 5
                phaseVocoder->setParams(params);
                phaseVocoder->processMono(input, output, numSamples);
                break;
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

} // namespace VSE
