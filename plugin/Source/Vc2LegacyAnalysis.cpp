// The VC-2's LEGACY analysis: see Vc2LegacyAnalysis.h (v210).  Every address is rom.4's; the check is xt_vc2legacy's
// (Vc2LegacyCheck.cpp).  The arithmetic is the SH-4's (XtFloat): an FMAC is the product rounded, then the sum.
#include "Vc2LegacyAnalysis.h"
#include "Vc2RomTables.h"
#include "XtFloat.h"
#include <bit>

namespace {
// [FW] .data: the bands' coefficients' pointer and the band count; the hold's coefficients, 51…100 reached
constexpr uint32_t kCoefficientsAt = 0x8994024c, kCount = 0x89940250;
constexpr Vc2Span kHoldTable { 0x89561538, 101 };
constexpr int kBandCoefficients = 5;
// [FW 0x893ef898…0x893ef8b8, 0x893efb7c…0x893efbd0] rom.4's bit patterns
constexpr float kLetGoStep = std::bit_cast<float>(0xbc23d709u);     // −0.01 a block
constexpr float kGlideTake = std::bit_cast<float>(0x3c23d709u);     // 0.01: the glide back's weights
constexpr float kGlideKeep = std::bit_cast<float>(0x3f7d70a3u);     // 0.99
constexpr float kRelease = std::bit_cast<float>(0x3f7e750cu);       // 0.993973: the follower, falling
constexpr float kAttackKeep = std::bit_cast<float>(0x3f6ffe77u);    // 0.937477: rising
constexpr float kAttackTake = std::bit_cast<float>(0x3d68d088u);    // 0.0568395
constexpr float kLowpassIn = std::bit_cast<float>(0x379383afu);     // 1.75851e-5: the band's lowpass, (1, 2, 1) over
constexpr float kLowpassBack1 = std::bit_cast<float>(0x3ffe3e73u);  // 1.98628
constexpr float kLowpassBack2 = std::bit_cast<float>(0xbf7c8182u);  // −0.986351
constexpr float kPathIn = std::bit_cast<float>(0x3e4a075au);        // 0.197294: the unvoiced path's one-pole lowpass
constexpr float kPathKeep = std::bit_cast<float>(0x3f4bdc8bu);      // 0.796334
constexpr float kPathLeak = std::bit_cast<float>(0x3f0b91b7u);      // 0.545192: its first difference's
// its three (1 − z⁻¹)² sections' feedback, on the last sample and the one before
constexpr float kSections[3][2] = {
    { std::bit_cast<float>(0x3f834712u), std::bit_cast<float>(0xbe98d980u) },
    { std::bit_cast<float>(0x3f92bc7du), std::bit_cast<float>(0xbece40b9u) },
    { std::bit_cast<float>(0x3fb439abu), std::bit_cast<float>(0xbf29c9d7u) },
};
constexpr float kHoldScale = std::bit_cast<float>(0x3c820821u);     // 1/63
constexpr float kFreeze = std::bit_cast<float>(0x3f7d70a3u);        // 0.99: past it the held envelope stops
constexpr float kTableSlope = 50.0f, kTableAt = 51.0f;

struct Tables {
    int count = 0;   // the bands that run, at most 31: the row after them is the unvoiced path's
    float coefficients[(Vc2LegacyAnalysisWords::kSlots - 1) * kBandCoefficients] {};
    float hold[kHoldTable.floats] {};
    bool read(Sh4& image) {
        count = (int)image.rd32(kCount);
        if (count < 0 || count >= Vc2LegacyAnalysisWords::kSlots) return false;
        vc2ReadFloats(image, { image.rd32(kCoefficientsAt), count * kBandCoefficients }, coefficients);
        vc2ReadFloats(image, kHoldTable, hold);
        return true;
    }
};

constinit Vc2RomTables<Tables> g_tables;
} // namespace

bool vc2LoadLegacy(const char* path) { return g_tables.load(path); }
bool vc2LegacyLoaded() { return g_tables.loaded(); }

void Vc2LegacyAnalysis::reset() {
    s = {};
    s.take = 1.0f;
}

void Vc2LegacyAnalysis::holdFrom(int controller) {
    using namespace xtf;
    const float f = mul(mul(flt(controller - 64), dump), kHoldScale);
    if (0.0f > f) {
        level = s.held = 1.0f;
        return;
    }
    level = sub(1.0f, f);
    if (f > kFreeze) s.held = 0.0f;
    else if (f == 0.0f) s.held = 1.0f;
    else {
        // f is in (0, 0.99] here, so the entry is 51…100; only a dump time of NaN reaches past it, and leaves it
        const int at = ftrc(add(mul(f, kTableSlope), kTableAt));
        if (at >= 0 && at < kHoldTable.floats) s.held = g_tables.t.hold[at];
    }
}

void Vc2LegacyAnalysis::hold(int controller) {
    s.controller = controller;
    holdFrom(controller);
}

void Vc2LegacyAnalysis::dumpTime(float d) {
    dump = d;
    holdFrom(s.controller);
}

void Vc2LegacyAnalysis::weights() {
    using namespace xtf;
    if (s.controller >= 64) {
        s.letGo = 1.0f;
        s.take = s.held;
        s.keep = sub(1.0f, s.held);
        return;
    }
    const float g = add(s.letGo, kLetGoStep);
    const bool gliding = g > 0.0f;
    s.letGo = gliding ? g : 0.0f;
    s.take = gliding ? kGlideTake : 1.0f;
    s.keep = gliding ? kGlideKeep : 0.0f;
}

float Vc2LegacyAnalysis::band(int b, float u) {
    using namespace xtf;
    const float* c = &g_tables.t.coefficients[b * kBandCoefficients];
    float* st = s.bands[b];
    const float y = add(mul(c[0], u), st[0]);
    st[0] = add(mul(c[1], y), st[1]);
    st[1] = mul(c[2], y);
    const float z = add(st[2], y);
    st[2] = add(mul(c[3], z), st[3]);
    st[3] = mul(c[4], z);
    st[11] = st[11] > z ? mul(st[11], kRelease) : add(mul(z, kAttackTake), mul(st[11], kAttackKeep));
    st[12] = add(mul(st[11], s.take), mul(st[12], s.keep));
    const float w1 = st[8], w2 = st[9];
    const float w = add(mul(w2, kLowpassBack2), add(mul(st[12], kLowpassIn), mul(w1, kLowpassBack1)));
    st[9] = w1;
    st[8] = w;
    return add(add(mul(w1, 2.0f), w), w2);
}

float Vc2LegacyAnalysis::unvoiced(float x) {
    using namespace xtf;
    float* p = s.path;
    const float lowpassed = add(mul(x, kPathIn), mul(p[0], kPathKeep));
    const float d = sub(lowpassed, p[0]);
    p[0] = lowpassed;
    const float leaked = add(mul(p[1], kPathLeak), d);
    float y = sub(leaked, p[1]);
    p[1] = leaked;
    for (const auto& feedback : kSections) {
        float* w = &p[2 + 2 * (&feedback - kSections)];   // w[0] the section's last, w[1] the one before
        const float a = w[0], b = w[1];
        const float v = add(mul(b, feedback[1]), add(mul(a, feedback[0]), y));
        y = add(sub(v, mul(a, 2.0f)), b);
        w[1] = a;
        w[0] = v;
    }
    return y;
}

void Vc2LegacyAnalysis::block(const float* in) {
    using namespace xtf;
    weights();
    const int count = g_tables.t.count;
    for (int i = 0; i < kBlock; ++i) {
        const float x = in[i];
        const float u = add(sub(x, mul(s.last, 2.0f)), s.beforeLast);
        s.beforeLast = s.last;
        s.last = x;
        for (int b = 0; b < count; ++b) s.rows[b][i] = band(b, u);
        s.rows[count][i] = unvoiced(x);
    }
}
