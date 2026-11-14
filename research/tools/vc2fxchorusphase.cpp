// The Chorus's LFO on both images (vc2fxchorus.h; research/VC2_VOCAL_DESIGNER.md, "The Chorus's sides"): each side's
// delay through the cycle, the lag that lines rom.4's up with rom.2's, and whether a reload sets the phase.
#include "vc2fxchorus.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace vc2 {

namespace {

// An impulse every kStep samples, kPoints of them (more than a cycle at the defaults' Rate), each traced for kWindow
constexpr int kPoints = 3000, kStep = 64, kWindow = 96;
// How far the reload test runs the chip on before it reloads: not a whole number of the defaults' cycles
constexpr int kRunBetween = 51200;

// Each side's delay at each point: the response's centre over its taps (each sample at least kTapShare of its largest,
// weighted by its size), so a read between two samples gives the fraction
struct Track {
    std::vector<double> left, right;
};

double centreOf(const std::vector<int32_t>& hit, const std::vector<int32_t>& none) {
    double most = 0, sum = 0, weighted = 0;
    for (size_t t = 0; t < hit.size(); ++t) most = std::max(most, std::fabs((double)hit[t] - none[t]));
    for (size_t t = 0; t < hit.size(); ++t) {
        const double x = std::fabs((double)hit[t] - none[t]);
        if (x < kTapShare * most) continue;
        sum += x;
        weighted += x * (double)t;
    }
    return sum > 0 ? weighted / sum : -1;
}

Track trackOf(const XtFxChip& chip) {
    auto c = settled(chip);
    Track k;
    for (int p = 0; p < kPoints; ++p) {
        const auto hit = tracesOf(*c, kChorusSend, {0xf0, 0xf2}, kWindow);
        const auto none = tracesOf(*c, kChorusSend, {0xf0, 0xf2}, kWindow, 0);
        k.left.push_back(centreOf(hit[0], none[0]));
        k.right.push_back(centreOf(hit[1], none[1]));
        runOn(*c, kStep);
    }
    return k;
}

void sayTrack(const char* image, const Track& k) {
    constexpr int kSaid = 30;
    const auto [lo, hi] = std::ranges::minmax(k.left);
    const auto [rlo, rhi] = std::ranges::minmax(k.right);
    std::printf("  %s: left %.2f…%.2f, right %.2f…%.2f samples; left/right every %d samples:", image, lo, hi, rlo, rhi,
                kPoints / kSaid * kStep);
    for (int p = 0; p < kPoints; p += kPoints / kSaid) std::printf(" %.1f/%.1f", k.left[(size_t)p], k.right[(size_t)p]);
    std::printf("\n");
}

// The lag that lines `later` up with `earlier` best, to kStep: `later` at a point stands where `earlier` stands
// `samples` on; its sides as they are or crossed, and the mean distance of a side's delay there, over the points both
// tracks have
struct Lag {
    int samples;
    double distance;
};

Lag lagOf(const Track& later, const Track& earlier, bool crossed) {
    const auto& a = crossed ? earlier.right : earlier.left;
    const auto& b = crossed ? earlier.left : earlier.right;
    Lag best{0, 1e9};
    for (int lag = -kPoints / 2; lag < kPoints / 2; ++lag) {
        double sum = 0;
        int n = 0;
        for (int p = std::max(0, -lag); p < kPoints && p + lag < kPoints; ++p, ++n)
            sum += std::fabs(later.left[(size_t)p] - a[(size_t)(p + lag)]) +
                   std::fabs(later.right[(size_t)p] - b[(size_t)(p + lag)]);
        if (n > kPoints / 4 && sum / n / 2 < best.distance) best = {lag * kStep, sum / n / 2};
    }
    return best;
}

const char* sidesName(bool crossed) {
    return crossed ? "crossed" : "as they are";
}

// Equal samples of the two chips' Chorus outs over kWindow samples after an impulse, rom.2's sides crossed or not
int equalOuts(const XtFxChip& rom2, const XtFxChip& rom4, bool crossed) {
    const auto a = tracesOf(rom2, kChorusSend, {0xf0, 0xf2}, kWindow);
    const auto b = tracesOf(rom4, kChorusSend, {0xf0, 0xf2}, kWindow);
    int equal = 0;
    for (size_t side = 0; side < b.size(); ++side)
        for (size_t t = 0; t < b[side].size(); ++t) equal += b[side][t] == a[crossed ? 1 - side : side][t] ? 1 : 0;
    return equal;
}

// The samples where the Chorus's outs differ (rom.2's sides crossed or not): the first kSaid, with both values
void sayDiffering(const Responses& rom4, const Responses& rom2, bool crossed) {
    constexpr int kSaid = 32;
    for (size_t side = 0; side < 2; ++side) {
        const auto& x = rom4[side];
        const auto& y = rom2[crossed ? 1 - side : side];
        std::printf("    rom.4 %s against rom.2's, where they differ (sample: rom.4, rom.2):", side ? "f2" : "f0");
        int said = 0;
        for (size_t t = 0; t < x.size() && said < kSaid; ++t) {
            if (x[t] == y[t]) continue;
            std::printf(" %zu: %.0f, %.0f;", t, x[t], y[t]);
            ++said;
        }
        std::printf("\n");
    }
}

// The lag to the sample near `lag` (a track's): the chip behind run on a sample at a time over two of the track's steps
// until the Chorus outs agree most; then the whole response there, rom.4's against rom.2's
void sayLinedUp(const XtFxChip& rom2, const XtFxChip& rom4, int lag, bool crossed) {
    const bool rom4Behind = lag < 0;
    auto a = settled(rom2), b = settled(rom4);
    XtFxChip& behind = rom4Behind ? *b : *a;
    const int from = std::max(0, std::abs(lag) - kStep);
    runOn(behind, from);
    int best = -1, bestAt = from;
    for (int d = 0; d <= 2 * kStep; ++d) {
        const int equal = equalOuts(*a, *b, crossed);
        if (equal > best) {
            best = equal;
            bestAt = from + d;
        }
        runOn(behind, 1);
    }
    std::printf("    to the sample: %s run on %d samples, %d of %d samples of the Chorus's outs equal; then:\n",
                rom4Behind ? "rom.4" : "rom.2", bestAt, best, 2 * kWindow);
    auto a2 = settled(rom2), b2 = settled(rom4);
    runOn(rom4Behind ? *b2 : *a2, bestAt);
    const Responses r2 = responsesOf(*a2), r4 = responsesOf(*b2);
    sayAgainst(r4, r2);
    sayDiffering(r4, r2, crossed);
}

class Rom4Chorus final : public ChorusImage {
public:
    Rom4Chorus(Machine& m, XtFxChip& chip, FxSettings& answers) : m_(m), chip_(chip), answers_(answers) {}
    const char* name() const override { return "rom.4"; }
    const XtFxChip& chip() const override { return chip_; }
    void runOn(int samples) override { vc2::runOn(chip_, samples); }
    bool set(int, int rom4Id, int value) override {
        answers_[rom4Id] = value;
        return reloadFx(m_);
    }

private:
    Machine& m_;
    XtFxChip& chip_;
    FxSettings& answers_;
};

// rom.2's chip runs on through XtFxUnit::process, on a silent frame
class Rom2Chorus final : public ChorusImage {
public:
    explicit Rom2Chorus(std::unique_ptr<XtFxUnit> unit) : unit_(std::move(unit)) {}
    const char* name() const override { return "rom.2"; }
    const XtFxChip& chip() const override { return unit_->chip(); }
    void runOn(int samples) override {
        constexpr int kBlock = 512, kFrameFloats = 8;
        std::vector<float> frames(kBlock * kFrameFloats), lr(2 * kBlock);
        for (int done = 0; done < samples; done += kBlock)
            unit_->process(frames.data(), lr.data(), std::min(kBlock, samples - done));
    }
    bool set(int rom2Id, int, int value) override { return loadRom2Fx(*unit_, {{rom2Id, value}}); }

private:
    std::unique_ptr<XtFxUnit> unit_;
};

// [FW] each reload's setting, by rom.2's ID and rom.4's (vc2fxchorus.cpp's): the value it's reloaded with, then the
// default it's set back to
struct Away {
    const char* what;
    int rom2Id, rom4Id, away, back;
};
const Away kAways[] = {
    {"Depth 0", 0x7c8, 0x7d5, 0, 10},
    {"the Chorus Switch off", 0x121, 0x13e, 0, 1},
    {"Chorus Type 2", 0x1fc, 0x23c, 2, 1},
};

} // namespace

void sayPhase(const XtFxChip& rom2, const XtFxChip& rom4) {
    const Track t2 = trackOf(rom2), t4 = trackOf(rom4);
    sayTrack("rom.2", t2);
    sayTrack("rom.4", t4);
    for (const bool crossed : {false, true}) {
        const Lag l = lagOf(t4, t2, crossed);
        std::printf("  rom.4 against rom.2, sides %s: rom.4 stands where rom.2 does %+d samples on, %.4f apart\n",
                    sidesName(crossed), l.samples, l.distance);
        sayLinedUp(rom2, rom4, l.samples, crossed);
    }
}

std::unique_ptr<ChorusImage> rom4Chorus(Machine& m, XtFxChip& chip, FxSettings& answers) {
    return std::make_unique<Rom4Chorus>(m, chip, answers);
}

std::unique_ptr<ChorusImage> rom2Chorus(std::unique_ptr<XtFxUnit> unit) {
    return std::make_unique<Rom2Chorus>(std::move(unit));
}

bool sayReloads(ChorusImage& c) {
    for (const Away& a : kAways) {
        const Track before = trackOf(c.chip());
        c.runOn(kRunBetween);
        if (!c.set(a.rom2Id, a.rom4Id, a.away) || !c.set(a.rom2Id, a.rom4Id, a.back)) {
            std::printf("fxchorus: %s's driver faulted\n", c.name());
            return false;
        }
        const Lag l = lagOf(trackOf(c.chip()), before, false);
        std::printf("  %s run on %d samples, then %s and back: the LFO %+d samples on, %.4f samples apart\n", c.name(),
                    kRunBetween, a.what, l.samples, l.distance);
    }
    return true;
}

} // namespace vc2
