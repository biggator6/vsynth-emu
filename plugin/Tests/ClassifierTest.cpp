// ClassifierTest.cpp: Auto's classifier (ContentClassifier.cpp) on two made-up waves of one pitch, a voiced tone and a
// pure one, which its band-energy test must tell apart.  The voiced tone was the legacy engine's tests' (EngineTests.cpp,
// test 5), kept when v191 removed that engine.  Built by research/tools/build.sh as classifier_test (no JUCE); verify.sh
// runs fcheck's copy.  Exit 0 if both hold.
#include "../Source/ContentClassifier.h"

#include <cmath>
#include <cstdio>
#include <iterator>
#include <numbers>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr double kSeconds = 2.0;

// A sine with −80 dB of dither: a mathematically perfect one is no real capture
std::vector<float> sine(double freq, double amp) {
    const int n = static_cast<int>(kSeconds * kRate);
    std::vector<float> v(static_cast<size_t>(n));
    unsigned rng = 12345;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const double dither = ((double)(rng >> 8) / (double)(1u << 24) - 0.5) * 2e-4;
        v[(size_t)i] = (float)(amp * std::sin(2.0 * std::numbers::pi * freq * i / kRate) + dither);
    }
    return v;
}

VSE::VariphraseAnalysis::ContentType classify(const std::vector<float>& x) {
    return VSE::analyzeContent(x.data(), static_cast<int>(x.size()), kRate).contentType;
}

}  // namespace

int main() {
    using CT = VSE::VariphraseAnalysis::ContentType;
    // F0 130 Hz with its 15th harmonic, 1950 Hz, in the 1–4 kHz band the classifier weighs: voiced, so SOLO
    auto voiced = sine(130.0, 0.4);
    const auto harmonic = sine(1950.0, 0.1);
    for (size_t i = 0; i < voiced.size(); ++i) voiced[i] += harmonic[i];
    // the same F0 with nothing in that band: LITE
    const auto pure = sine(130.0, 0.4);
    const struct { const char* name; const std::vector<float>& wave; CT want; } cases[] = {
        { "a voiced tone reads SOLO", voiced, CT::SOLO },
        { "a pure tone reads LITE", pure, CT::LITE },
    };
    int held = 0;
    for (const auto& c : cases) {
        const bool ok = classify(c.wave) == c.want;
        std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", c.name);
        held += ok ? 1 : 0;
    }
    std::printf("%d/%zu checks hold\n", held, std::size(cases));
    return held == static_cast<int>(std::size(cases)) ? 0 : 1;
}
