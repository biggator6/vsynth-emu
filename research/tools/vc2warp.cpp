// vc2run warp: HIRESO's slot 4 (0x893f15d0(this, side, ratio)), the Formant Shift's map of synthesis bands onto the
// analysis envelopes, run on the engine its own builder made.
//   warp diff <side> <ratio>       every word of the engine slot 4 changes, before and after
//   warp map <side> <ratio> ...    the side's pivot and map at each ratio
//   warp check [step]              vc2warplaw.cpp's model against slot 4, bit for bit: both sides, the templates'
//                                  pivots and more, ratios 0.5…2 every `step` (default 1/1024) and either side of each
//                                  branch's bound
#include "vc2runs.h"
#include "vc2warplaw.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

constexpr int kSlotWarp = 4;

std::vector<uint32_t> engineWords(Machine& m) {
    std::vector<uint32_t> w(kHiresoSize / 4);
    m.cpu.read(kHireso, w.data(), kHiresoSize);
    return w;
}

float asFloat(uint32_t u) {
    float f = 0;
    std::memcpy(&f, &u, 4);
    return f;
}

int diff(Machine& m, uint32_t side, float ratio) {
    const std::vector<uint32_t> before = engineWords(m);
    if (!runF(m, slotOf(m, kHireso, kSlotWarp), {kHireso, side}, ratio)) return 1;
    const std::vector<uint32_t> after = engineWords(m);
    int changed = 0;
    for (size_t i = 0; i < before.size(); ++i) {
        if (before[i] == after[i]) continue;
        std::printf("+%#06zx %08x %-12.7g -> %08x %.9g\n", i * 4, before[i], asFloat(before[i]), after[i],
                    asFloat(after[i]));
        ++changed;
    }
    std::printf("%d words changed, %llu steps\n", changed, (unsigned long long)m.cpu.steps());
    return 0;
}

constexpr uint32_t kMapAt = 0x2924, kMapStride = 0x200;   // [FW] side 0's map, side 1's 0x200 on (found by diff)
constexpr uint32_t kPivotAt = 0x2cb4;                       // [FW] a side's pivot p, an int a side (slot 5's ftrc)

// One line a ratio: the ratio, the side's pivot, then the fractional analysis band each synthesis band reads
int map(Machine& m, uint32_t side, int argc, char** argv) {
    for (int k = 0; k < argc; ++k) {
        const float ratio = std::strtof(argv[k], nullptr);
        if (!runF(m, slotOf(m, kHireso, kSlotWarp), {kHireso, side}, ratio)) return 1;
        std::printf("%.6g pivot %d:", ratio, (int)m.cpu.rd32(kHireso + kPivotAt + 4 * side));
        for (int b = 0; b < kWarpBands; ++b) std::printf(" %.7g", m.f32(kHireso + kMapAt + kMapStride * side + 4 * b));
        std::printf("\n");
    }
    return 0;
}

// The ratios checked: a grid, and the floats either side of 0.95, 1 and 1.05 (the branches' bounds) and of the ends
std::vector<float> checkRatios(float step) {
    std::vector<float> r;
    for (int i = 0; 0.5f + step * (float)i <= 2.0f; ++i) r.push_back(0.5f + step * (float)i);
    for (const float bound : {0.5f, 0.95f, 1.0f, 1.05f, 2.0f}) {
        r.push_back(std::nextafter(bound, 0.0f));
        r.push_back(bound);
        r.push_back(std::nextafter(bound, 4.0f));
    }
    return r;
}

// One side at pivot p over the ratios: the bands that differ from the model, the first few printed
long checkSide(Machine& m, const WarpConstants& k, int p, uint32_t side, const std::vector<float>& ratios) {
    const uint32_t slot = slotOf(m, kHireso, kSlotWarp);
    m.cpu.wr32(kHireso + kPivotAt + 4 * side, (uint32_t)p);
    long differ = 0;
    for (const float ratio : ratios) {
        if (!runF(m, slot, {kHireso, side}, ratio)) return -1;
        const std::array<float, kWarpBands> want = warpModel(k, p, ratio);
        for (int b = 0; b < kWarpBands; ++b) {
            const uint32_t at = kHireso + kMapAt + kMapStride * side + 4 * (uint32_t)b;
            uint32_t bits = 0;
            std::memcpy(&bits, &want[(size_t)b], 4);
            if (m.cpu.rd32(at) == bits) continue;
            if (++differ <= 3)
                std::printf("p %d side %u ratio %.9g band %d: firmware %.9g, model %.9g\n", p, side, ratio, b,
                            m.f32(at), want[(size_t)b]);
        }
    }
    return differ;
}

int check(Machine& m, float step) {
    const WarpConstants k = warpConstants(m);
    const std::vector<float> ratios = checkRatios(step);
    long cases = 0, differ = 0;
    for (const int p : {0, 1, 3, 6, 9, 12, 18, 20, 22, 24, 40, 63, 127}) {   // the templates' 1…24 and past them
        for (uint32_t side = 0; side < 2; ++side) {
            const long d = checkSide(m, k, p, side, ratios);
            if (d < 0) return 1;
            differ += d;
            cases += (long)ratios.size();
        }
    }
    std::printf("warp check: %ld cases (%d bands each), %ld bands different; %llu steps\n", cases, kWarpBands, differ,
                (unsigned long long)m.cpu.steps());
    return differ == 0 ? 0 : 1;
}

} // namespace

int warp(Machine& m, int argc, char** argv) {
    if (!m.run(kBuild, {kVocoder})) return 1;
    if (argc >= 4 && std::strcmp(argv[1], "diff") == 0)
        return diff(m, (uint32_t)std::strtoul(argv[2], nullptr, 0), std::strtof(argv[3], nullptr));
    if (argc >= 4 && std::strcmp(argv[1], "map") == 0)
        return map(m, (uint32_t)std::strtoul(argv[2], nullptr, 0), argc - 3, argv + 3);
    if (argc >= 2 && std::strcmp(argv[1], "check") == 0)
        return check(m, argc >= 3 ? std::strtof(argv[2], nullptr) : 1.0f / 1024);
    std::fprintf(stderr, "warp diff <side> <ratio> | warp map <side> <ratio> ... | warp check [step]\n");
    return 2;
}

} // namespace vc2
