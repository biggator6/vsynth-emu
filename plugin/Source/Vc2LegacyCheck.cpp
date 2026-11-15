// xt_vc2legacy <rom.4> [cases] [seed]: LEGACY against rom.4's own in Sh4Interp.  The analysis (v210): each case runs
// both inits (0x893ef6a0), then blocks of Vc2Signal's input (silence, noise and sines from almost nothing to full scale,
// cuts, long silences; every eighth case noise faded into silence) through both analyses (0x893ef740), with the Hold
// pedal's 0…127 (0x893efa70) and a new Hold Dump Time (0x893efaf0) now and then between them; after every block and
// event the whole state (0x8a1bd9d4…0x8a1bf1ac) and the two .data words (0x89940254, 0x89940258) word for word, after a
// block besides a row through the reader (0x893efa00) and the level's getter (0x893efbe0).  Then the object (v211,
// Vc2LegacySynthCheck.cpp), from the state the analysis's cases leave, as rom.4's globals carry it.  The host flushes
// denormals, as the SH-4 does.  XT_VC2LEGACY_FIRST=1 says where the first case to differ first does, XT_VC2LEGACY_STOP=1
// stops there.  XT_VC2LEGACY_MAIN only.
#ifdef XT_VC2LEGACY_MAIN
#include "Vc2LegacyCheck.h"
#include "XtSwitches.h"
#include <cstdio>

namespace {
constexpr uint32_t kInit = 0x893ef6a0, kAnalysis = 0x893ef740, kRow = 0x893efa00;   // [FW]
constexpr uint32_t kHold = 0x893efa70, kDumpTime = 0x893efaf0, kLevel = 0x893efbe0;   // [FW]

// The Hold pedal's 0…127 or a new dump time, on both
std::string eventOn(Vc2Fw& fw, Vc2LegacyAnalysis& port, std::mt19937& rng) {
    if (rng() % 3 == 0) {
        const float dump = vc2DrawDump(rng);
        fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(dump);
        if (!fw.call(kDumpTime, {})) return "the dump time faults: " + fw.cpu.error();
        port.dumpTime(dump);
        return vc2LegacyStateDifference(fw, port);
    }
    const int controller = vc2DrawController(rng);
    if (!fw.call(kHold, { (uint32_t)controller })) return "the hold faults: " + fw.cpu.error();
    port.hold(controller);
    return vc2LegacyStateDifference(fw, port);
}

// One block through both analyses, then the first word that differs, a drawn row's sample and the level's getter
std::string analysisOn(Vc2Fw& fw, Vc2LegacyAnalysis& port, const float* x, std::mt19937& rng) {
    fw.cpu.write(kVc2LegacyBlockAt, x, sizeof(float) * Vc2LegacyAnalysis::kBlock);
    if (!fw.call(kAnalysis, { kVc2LegacyBlockAt })) return "the analysis faults: " + fw.cpu.error();
    port.block(x);
    std::string what = vc2LegacyStateDifference(fw, port);
    const int band = (int)(rng() % 11), n = (int)(rng() % Vc2LegacyAnalysis::kBlock);
    const bool row = fw.call(kRow, { (uint32_t)band, (uint32_t)n }) && vc2SameAsFr0(fw, port.row(band, n));
    if (what.empty() && !row) what = "the row reader";
    const bool level = fw.call(kLevel, {}) && vc2SameAsFr0(fw, port.holdLevel());
    if (what.empty() && !level) what = "the level's getter";
    return what;
}

struct Counts {
    long blocks = 0, events = 0, held = 0, frozen = 0, gliding = 0, different = 0;
};

// The analysis's cases, on `port`
long analysisCases(Vc2Fw& fw, Vc2LegacyAnalysis& port, const Vc2Run& run) {
    std::mt19937 rng((uint32_t)run.seed);
    Counts n;
    for (int c = 0; c < run.cases; ++c) {
        const bool fades = c % kVc2LegacyFadeEvery == kVc2LegacyFadeEvery - 1;
        Vc2Signal input { std::mt19937(rng()), fades };
        std::string what = vc2ResetOn(fw, kInit, {}, port, vc2LegacyStateDifference);
        float x[Vc2LegacyAnalysis::kBlock];
        for (int k = 0; k < (fades ? kVc2FadeBlocks : kVc2LegacyBlocks) && what.empty(); ++k, ++n.blocks) {
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
        if (!n.different++ && vc2LegacyFirst()) std::printf("  case %d, block %ld: %s\n", c, n.blocks, what.c_str());
        if (xtSwitch("XT_VC2LEGACY_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("legacy analysis: %d cases, %ld blocks, %ld events (%ld blocks held, %ld frozen, %ld gliding back), "
                "%ld different\n", run.cases, n.blocks, n.events, n.held, n.frozen, n.gliding, n.different);
    return n.different;
}
} // namespace

int vc2DrawController(std::mt19937& rng) {
    constexpr int kPicks[10] = { 127, 127, 127, 127, 0, 0, 0, 64, 63, -1 };   // -1: any
    const int pick = kPicks[rng() % 10];
    return pick < 0 ? (int)(rng() % 128) : pick;
}

float vc2DrawDump(std::mt19937& rng) {
    constexpr int kDumps[6] = { 127, 126, 0, -1, -1, -1 };   // -1: any
    const int pick = kDumps[rng() % 6];
    return (float)(pick < 0 ? (int)(rng() % 128) : pick) / 127.0f;
}

std::string vc2LegacyStateDifference(Vc2Fw& fw, const Vc2LegacyAnalysis& port) {
    std::string what = vc2FirstDifference(fw, kVc2LegacyWords, &port.s, sizeof port.s, "the state");
    if (what.empty()) what = vc2FirstDifference(fw, kVc2LegacyDumpWord, &port.dump, 4, "the dump time");
    if (what.empty()) what = vc2FirstDifference(fw, kVc2LegacyLevelWord, &port.level, 4, "the level");
    return what;
}

bool vc2LegacyFirst() { return xtSwitch("XT_VC2LEGACY_FIRST") != nullptr; }

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2legacy", vc2LoadLegacy);
    if (!run) return 1;
    Vc2LegacyAnalysis port;
    long different = analysisCases(fw, port, *run);
    for (int channels = 1; channels <= 2 && (!different || !xtSwitch("XT_VC2LEGACY_STOP")); ++channels)
        different += vc2LegacySynthesisCases(fw, port, *run, channels);
    std::printf("xt_vc2legacy: %ld different\n", different);
    return 0;
}
#endif
