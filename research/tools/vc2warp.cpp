// vc2run warp: HIRESO's slot 4 (0x893f15d0(this, side, ratio)), the Formant Shift's map of synthesis bands onto the
// analysis envelopes, run on the engine its own builder made.
//   warp diff <side> <ratio>       every word of the engine slot 4 changes, before and after
//   warp map <side> <ratio> ...    the side's pivot and map at each ratio
// Its check against the law, `warp check`, and the law's model (vc2warplaw.cpp) went into the plugin at v213: the law is
// plugin/Source/Vc2HiresoWarp.cpp's, held to slot 4 by xt_vc2hireso (Vc2HiresoCheck.cpp).
#include "vc2runs.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

constexpr int kSlotWarp = 4, kWarpBands = 32;

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

} // namespace

int warp(Machine& m, int argc, char** argv) {
    if (!m.run(kBuild, {kVocoder})) return 1;
    if (argc >= 4 && std::strcmp(argv[1], "diff") == 0)
        return diff(m, (uint32_t)std::strtoul(argv[2], nullptr, 0), std::strtof(argv[3], nullptr));
    if (argc >= 4 && std::strcmp(argv[1], "map") == 0)
        return map(m, (uint32_t)std::strtoul(argv[2], nullptr, 0), argc - 3, argv + 3);
    std::fprintf(stderr, "warp diff <side> <ratio> | warp map <side> <ratio> ... (the check is xt_vc2hireso's)\n");
    return 2;
}

} // namespace vc2
