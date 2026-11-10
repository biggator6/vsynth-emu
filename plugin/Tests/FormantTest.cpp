// The legacy engine's formant analysis keeps a smoothed autocorrelation from frame to frame (.claude/fix-plan.md,
// section 9; v176).  It was a function-local thread_local, so every SourceFilterModel processed on one thread (each
// processor's legacy engine, which plays a file on the LPC algorithm when there's no firmware image) smoothed into the
// same numbers, and a reset (the processor's on each note-on) kept them.  Two engines on the LPC algorithm, each a
// vowel with its formants shifted, processed block by block on one thread, must each give what it gives alone; and an
// engine reset and played again must give what it gave the first time.  The summary's checksum is the first engine's
// output alone: a change that keeps it is bit for bit.  v176's own move didn't keep it, by where the compiler fuses
// multiply-adds alone (bit for bit with -ffp-contract=off; at most -147 dBFS apart, notes.txt).
// Run: VSynthEmuFormantTest <rom.2.bin> (the image unused: the engine plays without it).  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "../Source/VariphraseEngine.h"
#include "../Source/XtFiles.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 48000.0;
constexpr int kBlock = 512, kBlocks = 94;   // 1 s
constexpr double kBandwidth = 90.0;         // Hz, each formant's
constexpr float kPeak = 0.3f;

// A vowel: a pulse train at f0 through three formants, the model's voiced speech (energy in its 2 kHz band)
struct Vowel {
    double f0;
    double formants[3];
    float shift;   // semitones, the engine's formant shift
};
constexpr Vowel kAh { 140.0, { 730.0, 1090.0, 2440.0 }, 7.0f };
constexpr Vowel kEe { 210.0, { 300.0, 2290.0, 3010.0 }, -5.0f };

std::vector<float> sing(const Vowel& v) {
    std::vector<double> x(static_cast<size_t>(kBlock) * kBlocks);
    double phase = 1.0;
    for (double& s : x) {
        s = phase >= 1.0 ? 1.0 : 0.0;
        phase = (phase >= 1.0 ? phase - 1.0 : phase) + v.f0 / kRate;
    }
    for (const double f : v.formants) {   // two poles each, in cascade
        const double r = std::exp(-std::numbers::pi * kBandwidth / kRate);
        const double c = 2.0 * r * std::cos(2.0 * std::numbers::pi * f / kRate);
        double y1 = 0.0, y2 = 0.0;
        for (double& s : x) {
            const double y = s + c * y1 - r * r * y2;
            y2 = y1;
            y1 = y;
            s = y;
        }
    }
    double peak = 0.0;
    for (const double s : x) peak = std::max(peak, std::abs(s));
    std::vector<float> out(x.size());
    std::ranges::transform(x, out.begin(), [peak](double s) { return static_cast<float>(s / peak * kPeak); });
    return out;
}

// An engine on the LPC algorithm playing a vowel, block by block, what it gives kept
class Singer {
public:
    explicit Singer(const Vowel& v) : in_(sing(v)) {
        engine_.prepare(kRate, kBlock, VSE::Algorithm::LPCSourceFilter, { .formantShiftSemitones = v.shift });
    }
    void block(int b) {
        std::vector<float> o(kBlock);
        const float* i[] = { in_.data() + static_cast<size_t>(b) * kBlock };
        float* const op[] = { o.data() };
        engine_.process(i, op, 1, kBlock);
        out.insert(out.end(), o.begin(), o.end());
    }
    void play() {
        for (int b = 0; b < kBlocks; ++b) block(b);
    }
    void reset() {
        engine_.reset();
        out.clear();
    }
    std::vector<float> out;

private:
    std::vector<float> in_;
    VSE::VariphraseEngine engine_;
};

// Where two outputs first differ, and how many samples do, or "" if none
std::string differs(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return std::to_string(a.size()) + " samples against " + std::to_string(b.size());
    size_t n = 0, first = a.size();
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == b[i]) continue;
        first = std::min(first, i);
        ++n;
    }
    return n ? std::to_string(n) + " samples differ, the first at " + std::to_string(first) : "";
}

int formantTest(const juce::File&, const juce::File&) {
    TestChecks check { "formants" };
    Singer alone(kAh);
    alone.play();
    double peak = 0.0;
    for (const float s : alone.out) peak = std::max(peak, static_cast<double>(std::abs(s)));
    check(peak > 0.01, "the vowel should sound through the LPC algorithm");

    Singer a(kAh), b(kEe);
    for (int i = 0; i < kBlocks; ++i) {
        a.block(i);
        b.block(i);
    }
    const std::string shared = differs(a.out, alone.out);
    check(shared.empty(), "an engine played beside another on its thread should give what it gives alone: " + shared);

    Singer again(kAh);
    again.play();
    const std::vector<float> first = again.out;
    again.reset();
    again.play();
    const std::string kept = differs(again.out, first);
    check(kept.empty(), "an engine reset and played again should give what it gave the first time: " + kept);

    char sum[64];
    std::snprintf(sum, sizeof sum, "alone %016llx",
                  static_cast<unsigned long long>(xtfile::fnv(alone.out.data(), alone.out.size() * sizeof(float))));
    return check.summary(sum);
}

} // namespace

int main(int argc, char** argv) {
    return testMain(argc, argv, "formants", formantTest);
}
