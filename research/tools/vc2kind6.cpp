// vc2run kind6: voice kind 6 (the EXT IN path's, render 0x893ec730), run on rom.4's own routines: the globals' setters
// (0x893ec6a0, 0x893ecef0, from the init table at 0x898c60a8), the mic ring's reset (0x893ec7d0) and mode (0x893ec890),
// the mic written in by the block mixer's routine (0x893ec820(input, 32)) with the tracker's period at 0x899d9bd0, a
// voice by its setter (0x893ec9a0(voice, P)) and start (0x893ec8d0).  The input is a wave of a known period; the
// output's period is measured by its autocorrelation.
//   kind6      mode 1 (P+0 a ratio to the input's pitch) and mode 0 (P+0 a pitch, as a ratio to 440 Hz), each at two
//              input periods: the output's period against what the reading of 0x893ecd50 says it should be
#include "vc2runs.h"
#include <cmath>
#include <cstdio>
#include <numbers>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kSetRates = 0x893ec6a0, kSetMicGlobals = 0x893ecef0;   // [FW] the init table's
constexpr uint32_t kResetRing = 0x893ec7d0, kSetMode = 0x893ec890;        // [FW] the mic ring
constexpr uint32_t kWriteRing = 0x893ec820;     // [FW] (input, n): n samples into the ring, the period into its history
constexpr uint32_t kTrackedPeriod = 0x899d9bd0; // [FW] what 0x893ee010 returns: the pitch tracker's period, samples
constexpr uint32_t kSetVoice = 0x893ec9a0, kStartVoice = 0x893ec8d0, kRender = 0x893ec730;
constexpr int kBlock = 32, kWarmBlocks = 64, kBlocks = 400, kTail = 8192;
constexpr float kA4Period = 44100.0f / 440.0f;  // mode 0's P+0 is a pitch as a ratio to 440 Hz (0x899d996c = 440/44100)

constexpr double kPeakShare = 0.85;   // the period is the first autocorrelation peak this near the strongest

// The output's period over the last kTail samples: the first lag in [lo, hi] whose autocorrelation is a local peak
// within kPeakShare of the strongest (a non-integer period repeats exactly only at a multiple), refined by a parabola
double periodOf(const std::vector<float>& x, int lo, int hi) {
    const size_t from = x.size() - kTail;
    std::vector<double> score((size_t)hi + 2, 0.0);
    double top = 0;
    for (int lag = lo - 1; lag <= hi + 1; ++lag) {
        double s = 0;
        for (size_t n = from + (size_t)hi + 1; n < x.size(); ++n) s += (double)x[n] * x[n - (size_t)lag];
        score[(size_t)lag] = s;
        top = std::fmax(top, s);
    }
    for (int lag = lo; lag <= hi; ++lag) {
        const double a = score[(size_t)lag - 1], b = score[(size_t)lag], c = score[(size_t)lag + 1];
        if (b < kPeakShare * top || b < a || b < c) continue;
        const double bend = a - 2 * b + c;
        return lag + (bend != 0 ? 0.5 * (a - c) / bend : 0.0);
    }
    return 0;
}

struct Case {
    uint32_t mode;
    int inputPeriod;
    float p0;
};

// One input block into the ring, as the block mixer writes the mic: a wave of two harmonics at the input period (a
// continuous one: a grain spans twice the shorter period from an epoch the firmware steps by the period, not one placed
// on a pulse, so a sparse pulse train can leave whole grains empty)
struct Feed {
    Machine& m;
    uint32_t in;
    int period;
    long n = 0;
    bool block() {
        for (int k = 0; k < kBlock; ++k) {
            const double ph = 2 * std::numbers::pi * (double)n / period;
            m.setF32(in + 4 * (uint32_t)k, (float)(0.3 * std::sin(ph) + 0.2 * std::sin(2 * ph + 1)));
            ++n;
        }
        return m.run(kWriteRing, {in, (uint32_t)kBlock});
    }
};

// The ring cleared and the mode set, from the other mode first: 0x893ec890 acts only on a change
bool ringFor(Machine& m, uint32_t mode) {
    if (!m.run(kResetRing, {})) return false;
    if (!m.run(kSetMode, {1 - mode})) return false;
    return m.run(kSetMode, {mode});
}

// A voice set from P (P+0 the pitch or ratio, the read rate 1, gain 1) and started
bool startVoice(Machine& m, uint32_t voice, float p0) {
    const uint32_t p = m.alloc(0x20);
    m.setF32(p + 0x0, p0);
    m.setF32(p + 0x4, 1.0f);
    m.setF32(p + 0x8, 1.0f);
    if (!m.run(kSetVoice, {voice, p})) return false;
    return m.run(kStartVoice, {voice});
}

// The voice's output for one case, after kWarmBlocks of input
std::vector<float> render(Machine& m, Case c) {
    std::vector<float> out;
    if (!ringFor(m, c.mode)) return out;
    const uint32_t voice = m.alloc(0x100);
    const uint32_t buf = m.alloc(8 * kBlock);
    m.setF32(kTrackedPeriod, (float)c.inputPeriod);
    Feed feed{m, m.alloc(4 * kBlock), c.inputPeriod};
    for (int b = 0; b < kWarmBlocks; ++b)
        if (!feed.block()) return out;
    if (!startVoice(m, voice, c.p0)) return out;
    for (int b = 0; b < kBlocks; ++b) {
        if (!feed.block() || !m.run(kRender, {voice, buf})) return {};
        for (int k = 0; k < kBlock; ++k) out.push_back(m.f32(buf + 8 * (uint32_t)k));
    }
    return out;
}

} // namespace

int kind6(Machine& m, int, char**) {
    if (!m.run(kSetRates, {}) || !m.run(kSetMicGlobals, {})) return 1;
    int wrong = 0;
    for (const Case c : {Case{1, 200, 1.0f}, Case{1, 200, 1.5f}, Case{1, 200, 0.5f}, Case{1, 300, 2.0f},
                         Case{0, 200, 1.0f}, Case{0, 300, 1.0f}, Case{0, 200, 0.5f}, Case{0, 300, 2.0f}}) {
        const std::vector<float> out = render(m, c);
        if (out.size() < (size_t)kTail) return 1;
        const double want = c.mode == 1 ? c.inputPeriod / c.p0 : kA4Period / c.p0;
        const double got = periodOf(out, 30, 1000);
        double peak = 0;
        for (const float v : out) peak = std::fmax(peak, std::fabs(v));
        const bool ok = std::fabs(got - want) <= 1.0;
        wrong += ok ? 0 : 1;
        std::printf("mode %u, input period %d, P+0 %.3g: output period %.2f, the reading says %.2f%s; peak %.4g\n",
                    c.mode, c.inputPeriod, c.p0, got, want, ok ? "" : "  <- differs", peak);
    }
    return wrong == 0 ? 0 : 1;
}

} // namespace vc2
