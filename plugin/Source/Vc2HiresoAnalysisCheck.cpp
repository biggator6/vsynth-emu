// xt_vc2hireso's third part (v215): HIRESO's analysis against rom.4's own on its .bss globals.  Each case runs both inits
// (0x893ed8f0), then blocks of Vc2Signal's input (silence, noise and sines from almost nothing to full scale, cuts, long
// silences; every eighth case noise faded into silence) through both analyses (0x893ed930), each block's flag drawn in
// runs and written into both input stages' rings (rom.4's 0x8a0f87dc, the port's Vc2Unvoiced) for 0x893ed3e0 to read;
// then both envelopes' updates (0x893edd10), as the synthesis runs them each block; and now and then slot 0's clear
// (0x893ede50) between blocks.  After every block, update and clear the state (0x8a1bbd2c…0x8a1bd9d4), the mic at fs/2
// and fs/4 and the decimators' states (0x899d9a88…0x899d9b70) word for word, and after a block a band's envelope
// through the getter (0x893eddf0).  One port object through the cases, as rom.4's globals: no init clears the
// decimators.  XT_VC2HIRESO_FIRST=1 says where the first case to differ does, XT_VC2HIRESO_STOP=1
// stops there.  XT_VC2HIRESO_MAIN only.
#ifdef XT_VC2HIRESO_MAIN
#include "Vc2HiresoAnalysis.h"
#include "Vc2HiresoCheck.h"
#include "Vc2Unvoiced.h"
#include "XtSwitches.h"
#include <algorithm>
#include <cstdio>

namespace {
constexpr uint32_t kInit = 0x893ed8f0, kAnalysis = 0x893ed930, kEnvelopes = 0x893edd10, kEnvelope = 0x893eddf0;  // [FW]
constexpr uint32_t kClear = 0x893ede50;                                                                 // [FW]
constexpr uint32_t kWords = 0x8a1bbd2c, kHalfAt = 0x899d9a88, kQuarterAt = 0x899d9ac8;                 // [FW]
constexpr uint32_t kToHalfStates = 0x899d9aec, kToQuarterStates = 0x899d9b30;   // [FW] after each one's table pointer
constexpr uint32_t kFlags = 0x8a0f87dc;                                          // [FW] the input stage's ring
constexpr uint32_t kBlockAt = 0x8C034000;   // the mic's block, in Vc2Fw's RAM
constexpr int kBlocks = 400, kFadeEvery = 8, kFlipOneIn = 6, kClearOneIn = 50, kTwiceOneIn = 8;

// The state, the mic at fs/2 and fs/4 and the decimators' states against rom.4's; empty when they are the same
std::string analysisDifference(Vc2Fw& fw, const Vc2HiresoAnalysis& a) {
    const struct {
        uint32_t at;
        const void* mine;
        size_t bytes;
        const char* what;
    } spans[] = {
        { kWords, &a.s, sizeof a.s, "the state" },
        { kHalfAt, a.half, sizeof a.half, "the mic at fs/2" },
        { kQuarterAt, a.quarter, sizeof a.quarter, "the mic at fs/4" },
        { kToHalfStates, a.toHalf.s, sizeof a.toHalf.s, "the first decimator" },
        { kToQuarterStates, a.toQuarter.s, sizeof a.toQuarter.s, "the second decimator" },
    };
    for (const auto& span : spans) {
        std::string what = vc2FirstDifference(fw, span.at, span.mine, span.bytes, span.what);
        if (!what.empty()) return what;
    }
    return "";
}

// A routine of no arguments on both, rom.4's and the port's (the envelopes' update, slot 0's clear), then the words
// that differ
std::string bothOn(Vc2Fw& fw, Vc2HiresoAnalysis& a, uint32_t routine, void (Vc2HiresoAnalysis::*mine)(),
                   const std::string& what) {
    if (!fw.call(routine, {})) return what + " faults: " + fw.cpu.error();
    (a.*mine)();
    const std::string differs = analysisDifference(fw, a);
    return differs.empty() ? "" : "after " + what + ", " + differs;
}

// One block through both analyses, then both envelopes' updates (now and then a second time on the same frame, which
// gives each band an energy equal to its largest, as a block played over and over would), each followed by the words
// that differ, then a drawn band's envelope through the getter
std::string hiresoBlockOn(Vc2Fw& fw, Vc2HiresoAnalysis& a, const Vc2Unvoiced& stage, const float* x,
                          std::mt19937& rng) {
    fw.cpu.write(kBlockAt, x, sizeof(float) * Vc2HiresoAnalysis::kBlock);
    if (!fw.call(kAnalysis, { kBlockAt })) return "the analysis faults: " + fw.cpu.error();
    a.block(x, stage.voiced());
    std::string what = analysisDifference(fw, a);
    for (int k = rng() % kTwiceOneIn == 0 ? 2 : 1; k > 0 && what.empty(); --k)
        what = bothOn(fw, a, kEnvelopes, &Vc2HiresoAnalysis::envelopes, "the envelopes");
    if (!what.empty()) return what;
    const int band = (int)(rng() % Vc2HiresoAnalysis::kBands);
    const bool same = fw.call(kEnvelope, { (uint32_t)band }) && vc2SameAsFr0(fw, a.envelope(band));
    return same ? "" : "the envelope's getter";
}
} // namespace

long vc2HiresoAnalysisCases(Vc2Fw& fw, const Vc2Run& run) {
    Vc2Cases cases { kInit, kFadeEvery, kBlocks, std::mt19937((uint32_t)run.seed) };
    std::mt19937& rng = cases.rng;
    const bool first = xtSwitch("XT_VC2HIRESO_FIRST") != nullptr;
    long unvoicedBlocks = 0, clears = 0, different = 0;
    Vc2HiresoAnalysis a;   // one through the cases, as rom.4's globals
    Vc2Unvoiced stage;     // its flags' ring alone, every word the block's flag, for its query
    bool unvoiced = false;
    for (int c = 0; c < run.cases; ++c) {
        const std::string what = vc2AnalysisCase(fw, a, analysisDifference, cases, c, [&](const float* x) {
            if (rng() % kClearOneIn == 0) {
                ++clears;
                const std::string said = bothOn(fw, a, kClear, &Vc2HiresoAnalysis::clear, "slot 0's clear");
                if (!said.empty()) return said;
            }
            if (rng() % kFlipOneIn == 0) unvoiced = !unvoiced;
            std::ranges::fill(stage.flags, unvoiced ? 1 : 0);
            fw.cpu.write(kFlags, stage.flags.data(), sizeof stage.flags);
            unvoicedBlocks += unvoiced ? 1 : 0;
            return hiresoBlockOn(fw, a, stage, x, rng);
        });
        if (what.empty()) continue;
        if (!different++ && first)
            std::printf("  analysis case %d, block %ld: %s\n", c, cases.blocksRun, what.c_str());
        if (xtSwitch("XT_VC2HIRESO_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("hireso analysis: %d cases, %ld blocks (%ld unvoiced), %ld clears, %ld different\n", run.cases,
                cases.blocksRun, unvoicedBlocks, clears, different);
    return different;
}
#endif
