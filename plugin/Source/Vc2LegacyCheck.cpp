// xt_vc2legacy <rom.4> [cases] [seed]: Vc2LegacyAnalysis against rom.4's own LEGACY analysis in Sh4Interp (v210).  Each
// case runs both inits (0x893ef6a0), then blocks of Vc2Signal's input (silence, noise and sines from almost nothing to
// full scale, cuts, long silences; every eighth case noise faded into silence) through both analyses (0x893ef740),
// with the Hold pedal's 0…127 (0x893efa70, slot 6) and a new Hold Dump Time (0x893efaf0, slot 5) now and then between
// them.  After every block and event the whole state (0x8a1bd9d4…0x8a1bf1ac) word for word and the two .data words,
// the dump time and the level (0x89940254, 0x89940258); after a block besides, a row and sample drawn through the reader
// (0x893efa00) and the level through its getter (0x893efbe0).  One analysis goes through the cases, as rom.4's globals
// do.  The host flushes denormals, as the SH-4 does.  XT_VC2LEGACY_FIRST=1 says where the first case to differ first
// does, XT_VC2LEGACY_STOP=1 stops there.  XT_VC2LEGACY_MAIN only.
#ifdef XT_VC2LEGACY_MAIN
#include "Vc2CheckRig.h"
#include "Vc2LegacyAnalysis.h"
#include "XtSwitches.h"
#include <bit>
#include <cstdio>
#include <random>

namespace {
constexpr uint32_t kInit = 0x893ef6a0, kAnalysis = 0x893ef740, kRow = 0x893efa00;   // [FW]
constexpr uint32_t kHold = 0x893efa70, kDumpTime = 0x893efaf0, kLevel = 0x893efbe0;   // [FW]
constexpr uint32_t kWords = 0x8a1bd9d4, kDumpWord = 0x89940254, kLevelWord = 0x89940258;   // [FW]
constexpr uint32_t kBlockAt = 0x8C011000;
constexpr int kBlocks = 400, kFadeEvery = 8;

// A getter's float result, as rom.4 leaves it in fr0, against the port's
bool sameAsFr0(const Vc2Fw& fw, float mine) { return fw.cpu.fb[0][0] == std::bit_cast<uint32_t>(mine); }

// The state and both .data words against rom.4's; empty when they are the same
std::string stateDifference(Vc2Fw& fw, const Vc2LegacyAnalysis& port) {
    std::string what = vc2FirstDifference(fw, kWords, &port.s, sizeof port.s, "the state");
    if (what.empty()) what = vc2FirstDifference(fw, kDumpWord, &port.dump, 4, "the dump time");
    if (what.empty()) what = vc2FirstDifference(fw, kLevelWord, &port.level, 4, "the level");
    return what;
}

// The Hold pedal's 0…127 (its two ends most of the time, as a pedal gives; 64 and 63, where the hold's branches part,
// now and then) or a new dump time, on both
std::string eventOn(Vc2Fw& fw, Vc2LegacyAnalysis& port, std::mt19937& rng) {
    if (rng() % 3 == 0) {
        // its ends most often: 127 and 126 put the pedal's hold either side of the freeze at 0.99, 0 holds nothing
        constexpr int kDumps[6] = { 127, 126, 0, -1, -1, -1 };   // -1: any
        const int pick = kDumps[rng() % 6];
        const float dump = (float)(pick < 0 ? (int)(rng() % 128) : pick) / 127.0f;
        fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(dump);
        if (!fw.call(kDumpTime, {})) return "the dump time faults: " + fw.cpu.error();
        port.dumpTime(dump);
        return stateDifference(fw, port);
    }
    constexpr int kPicks[10] = { 127, 127, 127, 127, 0, 0, 0, 64, 63, -1 };   // -1: any
    const int pick = kPicks[rng() % 10];
    const int controller = pick < 0 ? (int)(rng() % 128) : pick;
    if (!fw.call(kHold, { (uint32_t)controller })) return "the hold faults: " + fw.cpu.error();
    port.hold(controller);
    return stateDifference(fw, port);
}

// One block through both analyses, then the first word that differs, a drawn row's sample and the level's getter
std::string analysisOn(Vc2Fw& fw, Vc2LegacyAnalysis& port, const float* x, std::mt19937& rng) {
    fw.cpu.write(kBlockAt, x, sizeof(float) * Vc2LegacyAnalysis::kBlock);
    if (!fw.call(kAnalysis, { kBlockAt })) return "the analysis faults: " + fw.cpu.error();
    port.block(x);
    std::string what = stateDifference(fw, port);
    const int band = (int)(rng() % 11), n = (int)(rng() % Vc2LegacyAnalysis::kBlock);
    const bool row = fw.call(kRow, { (uint32_t)band, (uint32_t)n }) && sameAsFr0(fw, port.row(band, n));
    if (what.empty() && !row) what = "the row reader";
    const bool level = fw.call(kLevel, {}) && sameAsFr0(fw, port.holdLevel());
    if (what.empty() && !level) what = "the level's getter";
    return what;
}

struct Counts {
    long blocks = 0, events = 0, held = 0, frozen = 0, gliding = 0, different = 0;
};
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2legacy", vc2LoadLegacy);
    if (!run) return 1;
    std::mt19937 rng((uint32_t)run->seed);
    Counts n;
    const bool first = xtSwitch("XT_VC2LEGACY_FIRST") != nullptr;
    Vc2LegacyAnalysis port;
    for (int c = 0; c < run->cases; ++c) {
        const bool fades = c % kFadeEvery == kFadeEvery - 1;
        Vc2Signal input { std::mt19937(rng()), fades };
        std::string what = fw.call(kInit, {}) ? "" : "the init faults";
        port.reset();
        if (what.empty()) what = stateDifference(fw, port);
        float x[Vc2LegacyAnalysis::kBlock];
        for (int k = 0; k < (fades ? kVc2FadeBlocks : kBlocks) && what.empty(); ++k, ++n.blocks) {
            if (rng() % 24 == 0) {
                ++n.events;
                what = eventOn(fw, port, rng);
            }
            input.block(x);
            if (what.empty()) what = analysisOn(fw, port, x, rng);
            n.held += port.s.controller >= 64 ? 1 : 0;
            n.frozen += port.s.controller >= 64 && port.s.held == 0.0f ? 1 : 0;
            n.gliding += port.s.letGo > 0.0f && port.s.letGo < 1.0f ? 1 : 0;
        }
        if (what.empty()) continue;
        if (!n.different++ && first) std::printf("  case %d, block %ld: %s\n", c, n.blocks, what.c_str());
        if (xtSwitch("XT_VC2LEGACY_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("legacy analysis: %d cases, %ld blocks, %ld events (%ld blocks held, %ld frozen, %ld gliding back), "
                "%ld different\n", run->cases, n.blocks, n.events, n.held, n.frozen, n.gliding, n.different);
    std::printf("xt_vc2legacy: %ld different\n", n.different);
    return 0;
}
#endif
