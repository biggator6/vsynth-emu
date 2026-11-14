// vc2run <rom.4> fxchorus <rom.2>: whether rom.4's Chorus has the V-Synth's sides swapped
// (research/VC2_VOCAL_DESIGNER.md, "The Chorus's sides").  Chorus type 1 on both images, rom.2's through XtFxUnit and
// rom.4's through its own driver on an XtFxChip (vc2fxdriver.h): with Depth 0, so the LFO moves nothing, then with a
// Pre Delay and a Feedback, then at the type's defaults.  An impulse into the Chorus send (0x26); each word's response
// at the Chorus's outs (0xf0…0xf6) and the mix (0xa0, 0xa2), less the trace with no impulse: its first taps, and each
// of rom.4's words against each of rom.2's, how many samples are equal and the correlation.
// vc2run <rom.4> fxchorus <rom.2> lfo: at the defaults, the LFO's phase on both images and whether a reload sets it
// (vc2fxchorusphase.cpp).  Fails on a fault.
#include "vc2fxchorus.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace vc2 {

namespace {

// [FW] the switches (rom.2's 0x121, rom.4's 0x13e), the types (0x1fc, 0x23c), Chorus type 1's Pre Dly Time, Depth and
// Feedback (rom.2's 0x7c4, 0x7c8, 0x7c9; rom.4's 0x7d1, 0x7d5, 0x7d6); rom.4's EQ Hi Gain at rom.2's default, as
// vc2fxroute.cpp sets it.  `moving`: the LFO moves the taps, so the lfo run takes it
struct ChorusCase {
    const char* what;
    FxSettings rom2, rom4;
    bool moving;
};
const ChorusCase kChorusCases[] = {
    {"Chorus 1, Depth 0", {{0x121, 1}, {0x1fc, 1}, {0x7c8, 0}}, {{0x13e, 1}, {0x23c, 1}, {0x7d5, 0}, {0x70, 13}},
     false},
    {"Chorus 1, Depth 0, Pre Dly Time 50", {{0x121, 1}, {0x1fc, 1}, {0x7c8, 0}, {0x7c4, 50}},
     {{0x13e, 1}, {0x23c, 1}, {0x7d5, 0}, {0x7d1, 50}, {0x70, 13}}, false},
    {"Chorus 1, Depth 0, Feedback 98", {{0x121, 1}, {0x1fc, 1}, {0x7c8, 0}, {0x7c9, 98}},
     {{0x13e, 1}, {0x23c, 1}, {0x7d5, 0}, {0x7d6, 98}, {0x70, 13}}, false},
    {"Chorus 1 at its defaults (Rate 5, Depth 10)", {{0x121, 1}, {0x1fc, 1}}, {{0x13e, 1}, {0x23c, 1}, {0x70, 13}},
     true},
};
constexpr std::array<unsigned, 6> kOuts{0xf0, 0xf2, 0xf4, 0xf6, 0xa0, 0xa2};
constexpr int kFrames = 4096, kTapsSaid = 6;

void sayTaps(const char* image, const Responses& r) {
    for (size_t w = 0; w < kOuts.size(); ++w) {
        double most = 0;
        for (const double x : r[w]) most = std::max(most, std::fabs(x));
        std::printf("  %s %02x: largest %.0f;", image, kOuts[w], most);
        int said = 0;
        for (int t = 0; most > 0 && t < kFrames && said < kTapsSaid; ++t)
            if (std::fabs(r[w][(size_t)t]) >= kTapShare * most) {
                std::printf(" %d: %.0f", t, r[w][(size_t)t]);
                ++said;
            }
        std::printf("\n");
    }
}

} // namespace

Responses responsesOf(const XtFxChip& chip) {
    const auto quiet = settled(chip);
    const auto hit = tracesOf(*quiet, kChorusSend, {0xf0, 0xf2, 0xf4, 0xf6, 0xa0, 0xa2}, kFrames);
    const auto none = tracesOf(*quiet, kChorusSend, {0xf0, 0xf2, 0xf4, 0xf6, 0xa0, 0xa2}, kFrames, 0);
    Responses r(hit.size(), std::vector<double>(kFrames));
    for (size_t w = 0; w < hit.size(); ++w)
        for (int t = 0; t < kFrames; ++t) r[w][(size_t)t] = (double)hit[w][(size_t)t] - (double)none[w][(size_t)t];
    return r;
}

void sayAgainst(const Responses& rom4, const Responses& rom2) {
    for (size_t a = 0; a < kOuts.size(); ++a)
        for (size_t b = 0; b < kOuts.size(); ++b) {
            if ((a < 4) != (b < 4)) continue;   // the Chorus's outs against each other, the mix against itself
            double ab = 0, aa = 0, bb = 0;
            int equal = 0;
            for (int t = 0; t < kFrames; ++t) {
                const double x = rom4[a][(size_t)t], y = rom2[b][(size_t)t];
                ab += x * y;
                aa += x * x;
                bb += y * y;
                equal += x == y ? 1 : 0;
            }
            if (aa == 0 || bb == 0) continue;
            std::printf("  rom.4 %02x against rom.2 %02x: correlation %+.4f, %d of %d samples equal\n", kOuts[a],
                        kOuts[b], ab / std::sqrt(aa * bb), equal, kFrames);
        }
}

int fxchorus(Machine& m, int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "%s <rom.2> [lfo]\n", argv[0]);
        return 2;
    }
    const bool lfo = argc > 2 && std::string_view(argv[2]) == "lfo";
    FxSettings answers;
    std::set<int> heard;
    auto chip = std::make_unique<XtFxChip>();
    hookFxDriver(m, *chip, answers, heard);
    if (!startFxDriver(m)) {
        std::printf("fxchorus: rom.4's driver: %s\n", m.error().c_str());
        return 1;
    }
    for (const ChorusCase& k : kChorusCases) {
        if (lfo && !k.moving) continue;
        auto rom2 = rom2With(argv[1], k.rom2);
        answers = k.rom4;
        if (!rom2 || !reloadFx(m)) {
            std::printf("fxchorus: %s\n", m.error().c_str());
            return 1;
        }
        if (lfo) {
            std::printf("%s: the LFO's phase\n", k.what);
            sayPhase(rom2->chip(), *chip);
            if (!sayReloads(*rom4Chorus(m, *chip, answers)) || !sayReloads(*rom2Chorus(std::move(rom2)))) return 1;
            continue;
        }
        std::printf("%s: an impulse into the Chorus send\n", k.what);
        const Responses r2 = responsesOf(rom2->chip()), r4 = responsesOf(*chip);
        sayTaps("rom.2", r2);
        sayTaps("rom.4", r4);
        sayAgainst(r4, r2);
    }
    return 0;
}

} // namespace vc2
