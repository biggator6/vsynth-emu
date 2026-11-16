// HIRESO's object: see Vc2Hireso.h (v216).  Each routine's arithmetic in its own order, in XtFloat; every address is
// rom.4's.
#include "Vc2Hireso.h"
#include "XtFloat.h"
#include <algorithm>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;
using Tables = Vc2HiresoTables;
using Words = Vc2HiresoWords;

constexpr int kSides = 2;
constexpr int kBankCleared = 7;                 // [FW] the reset's: a band's first 7 of its 14
constexpr int kCarrierCoefs = 3;                // [FW] a carrier resonator's, a group's 2n + 1 resonators in turn
constexpr int kPivotField = 12;                 // [FW] the constructor's, both sides
constexpr int kTemplates = Tables::kRecords / kSides;   // template T: record T on the left, T + 16 on the right
constexpr uint32_t kCurveAt[kSides] = { 0x899d9d98, 0x899d9e18 };   // [FW] the RAM curves of templates 16 on
constexpr int kHoldCentre = 64;                 // [FW] slot 6's: the pedal from 64 up holds
// [FW] the constructor's settings for slot 5: Env Attack 6, Env Release 18, template 0, Vocoder Depth and Hold Dump Time
// 1, the pivots the template's
constexpr Vc2HiresoSettings kDefaults { {}, { -1, -1 }, {}, 6, 18, 0, 1.0f, 1.0f };
constexpr Vc2HiresoHoldWords kHoldReset { 0, 1.0f, 0.0f, { 0.0f, 1.0f, 0.0f } };   // [FW] the reset's
} // namespace

Vc2Hireso::Vc2Hireso() {
    for (Vc2RateFilter& f : rates) vc2RateClear(f);
    analysis.reset();
    reset();
    std::ranges::copy(kVc2HiresoEdges, s.edge);
    const Tables& tb = vc2HiresoTables();
    for (int g = 0; g < Words::kGroups; ++g)
        s.carrierTable[g] = tb.carrierAt + 4 * (uint32_t)(kCarrierCoefs * (2 * s.edge[g] + g));
    weights();
    for (int k = 0; k < kRates; ++k) rates[k].c = k < kRates / 2 ? tb.decimator : tb.interpolator;
    t.dump = 1.0f;   // the hold's words as the reset left them, which the constructor writes again
    std::ranges::fill(t.pivot, kPivotField);
    Vc2HiresoSettings defaults = kDefaults;
    settings(defaults);
}

void Vc2Hireso::weights() {
    const Tables& tb = vc2HiresoTables();
    const int last = Tables::kBands - 1;   // the constructor's band 31, whatever the edges
    const float scale = xtf::div(tb.weightScale, mul(tb.weightA[last], tb.weightB[last]));
    for (int b = s.edge[0]; b < s.edge[Words::kGroups]; ++b) {
        const float sign = xtf::flt(2 * (b & 1) - 1);
        s.weight[b] = mul(mul(sign, mul(tb.weightA[b], tb.weightB[b])), scale);
    }
}

void Vc2Hireso::reset() {
    analysis.clear();
    for (auto& band : s.bank) std::fill(band, band + kBankCleared, 0.0f);
    for (auto& row : s.bandWords) std::ranges::fill(row, 0.0f);
    std::fill(s.formant + Words::kBands, s.formant + Words::kSideWords, 0.0f);   // the three rows between the maps
    t.synthesis = 1.0f;
    t.hold = kHoldReset;
}

void Vc2Hireso::formant(int side, float ratio) {
    const Vc2HiresoMap map = vc2HiresoMapOf(t.pivot[side], ratio);
    std::ranges::copy(map, s.formant + Words::kSideWords * side);
}

void Vc2Hireso::settings(Vc2HiresoSettings& in) {
    const Tables& tb = vc2HiresoTables();
    // the converter's 0…100; one below 0 would read before rom.4's curve, which the port doesn't
    if (in.attack >= 0 && in.attack < Tables::kCurve) t.attack = tb.attack[in.attack];
    if (in.release >= 0 && in.release < Tables::kCurve) {
        const float release = tb.release[in.release];
        for (int b = 0; b < Tables::kBands; ++b) {
            const float cap = tb.release[tb.releaseCap[b]];
            t.release[b] = cap > release ? release : cap;
        }
    }
    t.dump = in.dump;
    hold(t.hold.controller);   // slot 6 through the vtable, on the dump time as it now stands
    t.depth = in.depth;
    formantTemplate(in);
    for (int side = 0; side < kSides; ++side)
        if (in.pivot[side] >= 0) t.pivot[side] = in.pivot[side];
}

void Vc2Hireso::formantTemplate(Vc2HiresoSettings& in) {
    const int k = in.formantTemplate;
    if (k < 0) return;
    if (k >= kTemplates) {   // the RAM curves an edit page writes
        std::ranges::copy(kCurveAt, t.templateAt);
        return;
    }
    const Tables& tb = vc2HiresoTables();
    for (int side = 0; side < kSides; ++side) {
        const int record = k + kTemplates * side;
        t.templateAt[side] = tb.templatesAt + Tables::kRecordBytes * (uint32_t)record + Tables::kRecordGains;
        if (in.pivot[side] < 0) in.pivot[side] = xtf::ftrc(tb.templatePivot[record]);
    }
}

void Vc2Hireso::hold(int controller) {
    const float* k = vc2HiresoTables().hold;
    t.hold.controller = controller;
    const float x = mul(mul(t.dump, xtf::flt(controller - kHoldCentre)), k[Tables::kHoldScale]);
    if (0.0f > x) {
        t.hold.level = t.hold.held = 1.0f;
        return;
    }
    t.hold.level = sub(1.0f, x);
    if (x > k[Tables::kHoldFreeze]) {
        t.hold.held = 0.0f;
    } else if (x == 0.0f) {
        t.hold.held = 1.0f;
    } else {
        const int at = xtf::ftrc(add(mul(x, k[Tables::kHoldStep]), k[Tables::kHoldBase]));
        t.hold.held = mul(vc2HiresoTables().release[at], k[Tables::kHoldGain]);
    }
}
