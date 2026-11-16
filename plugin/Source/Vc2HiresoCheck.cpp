// xt_vc2hireso <rom.4> [cases] [seed]: HIRESO against rom.4's own in Sh4Interp, on an object its constructor
// (0x893f0a10) makes in the interpreter's RAM.  The formant warp (v213): slot 4, 0x893f15d0(this, side, ratio), against
// vc2HiresoMapOf, the side's 32 bands (+0x2924, the other side's 0x200 on) bit for bit, for both sides, the templates'
// pivots 1…24 and some past them (written into the side's field, +0x2cb4 and +0x2cb8, as slot 5 would), and ratios from
// 0.5 to 2 on a grid of 1/1024 (every 40/cases-th of it) with the floats either side of 0.5, 0.95, 1, 1.05 and 2, the
// branches' bounds.  The grid and the pivots are research/tools/vc2warp.cpp's `warp check`'s, which held the law's first
// model to slot 4 and moved here with it.  Then the rate filters (v214, Vc2HiresoRateCheck.cpp).  XT_VC2HIRESO_FIRST=1
// says where the first map to differ does, XT_VC2HIRESO_STOP=1 stops there.  XT_VC2HIRESO_MAIN only.
#ifdef XT_VC2HIRESO_MAIN
#include "Vc2HiresoCheck.h"
#include "Vc2HiresoWarp.h"
#include "XtSwitches.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>

namespace {
constexpr uint32_t kConstruct = 0x893f0a10, kWarp = 0x893f15d0;               // [FW]
constexpr uint32_t kMapAt = 0x2924, kMapStride = 0x200, kPivotAt = 0x2cb4;   // [FW] in the object
constexpr uint32_t kObjAt = 0x8C020000;                                       // in Vc2Fw's RAM
constexpr int kPivots[] = { 0, 1, 3, 6, 9, 12, 18, 20, 22, 24, 40, 63, 127 };   // the templates' 1…24, and past them

// The ratios: the grid of 1/1024 from 0.5 to 2, every `every`th of it, and 0.5, 0.95, 1, 1.05 and 2 with the floats
// either side of each
std::vector<float> ratiosOf(int every) {
    constexpr float kStep = 1.0f / 1024;
    std::vector<float> r;
    for (int i = 0; 0.5f + kStep * (float)i <= 2.0f; i += every) r.push_back(0.5f + kStep * (float)i);
    for (const float bound : { 0.5f, 0.95f, 1.0f, 1.05f, 2.0f }) {
        r.push_back(std::nextafter(bound, 0.0f));
        r.push_back(bound);
        r.push_back(std::nextafter(bound, 4.0f));
    }
    return r;
}

// Slot 4 on both at pivot p and the ratio: the first band that differs, as text; empty when none does
std::string mapOn(Vc2Fw& fw, uint32_t side, int p, float ratio) {
    fw.cpu.wr32(kObjAt + kPivotAt + 4 * side, (uint32_t)p);
    fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(ratio);
    if (!fw.call(kWarp, { kObjAt, side })) return "slot 4 faults: " + fw.cpu.error();
    const Vc2HiresoMap map = vc2HiresoMapOf(p, ratio);
    char where[96];
    std::snprintf(where, sizeof where, "side %u, pivot %d, ratio %.9g: the map", side, p, ratio);
    return vc2FirstDifference(fw, kObjAt + kMapAt + kMapStride * side, map.data(), sizeof map, where);
}

struct Counts {
    long maps = 0, different = 0;
};

// Every side, pivot and ratio; false once XT_VC2HIRESO_STOP has stopped it
bool warpMaps(Vc2Fw& fw, const std::vector<float>& ratios, Counts& n) {
    const bool first = xtSwitch("XT_VC2HIRESO_FIRST") != nullptr;
    for (uint32_t side = 0; side < 2; ++side)
        for (const int p : kPivots)
            for (const float ratio : ratios) {
                ++n.maps;
                const std::string what = mapOn(fw, side, p, ratio);
                if (what.empty()) continue;
                if (!n.different++ && first) std::printf("  %s\n", what.c_str());
                if (xtSwitch("XT_VC2HIRESO_STOP")) return false;   // a plant's run: its first map that differs
            }
    return true;
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2hireso", vc2LoadHireso);
    if (!run) return 1;
    Counts n;
    const std::vector<float> ratios = ratiosOf(std::max(1, 40 / std::max(1, run->cases)));
    if (!fw.call(kConstruct, { kObjAt })) {
        std::printf("the constructor faults: %s\n", fw.cpu.error().c_str());
        ++n.different;
    } else {
        warpMaps(fw, ratios, n);
    }
    std::printf("hireso warp: %ld maps (2 sides, %zu pivots, %zu ratios), %ld different\n", n.maps, std::size(kPivots),
                ratios.size(), n.different);
    const long rates = n.different && xtSwitch("XT_VC2HIRESO_STOP") ? 0 : vc2HiresoRateCases(fw, *run);   // v214
    std::printf("xt_vc2hireso: %ld different\n", n.different + rates);
    return 0;
}
#endif
