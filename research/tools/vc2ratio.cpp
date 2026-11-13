// vc2run ratio: the Formant Shift ratio, the converter's 0x89506e54(2.0, (v − 64)/63) (0x893a7336…: v = 0 read as 1, then
// v · (1/63) + (−64/63) from its pool), run for every v and held to the host's powf; it is powf if every value is within
// 2 ulp (the SH-4 rounds to zero).  Then three other bases, to show it is a power and not a 2^x alone.
//   ratio      each v's ratio and its distance from powf in ulp; fails past 2 ulp
#include "vc2runs.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace vc2 {

namespace {

constexpr uint32_t kPow = 0x89506e54;                           // [FW] (fr4 base, fr5 exponent) → fr0
constexpr uint32_t kStepAt = 0x893a75f0, kOffsetAt = 0x893a75ec;   // [FW] the converter's 1/63 and −64/63
constexpr double kUlpLimit = 2.0;
constexpr int kValues = 128;

float powOf(Machine& m, float base, float exponent, bool& ok) {
    ok = runF(m, kPow, {}, base, exponent);
    float r = 0;
    std::memcpy(&r, &m.cpu.fb[0][0], 4);
    return r;
}

double ulpsFrom(float got, float want) {
    return (got - want) / (std::nextafter(want, INFINITY) - want);
}

} // namespace

int ratio(Machine& m, int, char**) {
    double worst = 0;
    bool ok = true;
    for (int v = 0; v < kValues; ++v) {
        const float x = (float)(v ? v : 1) * m.f32(kStepAt) + m.f32(kOffsetAt);
        const float r = powOf(m, 2.0f, x, ok);
        if (!ok) return 1;
        const double u = ulpsFrom(r, std::pow(2.0f, x));
        worst = std::fmax(worst, std::fabs(u));
        std::printf("v %3d: (v − 64)/63 = %.8f, ratio %.9g (%+.0f ulp from powf)\n", v, x, r, u);
    }
    for (const float base : {3.0f, 10.0f, 0.5f}) {
        for (const float e : {0.3f, -1.7f, 2.5f}) {
            const float r = powOf(m, base, e, ok);
            if (!ok) return 1;
            std::printf("%g^%g: %.9g, powf %.9g\n", base, e, r, std::pow(base, e));
        }
    }
    std::printf("ratio: the worst of %d values %.0f ulp from powf\n", kValues, worst);
    return worst <= kUlpLimit ? 0 : 1;
}

} // namespace vc2
