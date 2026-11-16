// xt_vc2hireso's fourth part (v216): HIRESO's object against rom.4's on an object of its own in the interpreter's RAM,
// zeroed first as the allocator's memory, the analysis's globals with it as at the start.  The constructor (0x893f0a10)
// on both, once; then each case, after the first, noise on both in the words only the synthesis uses, as it would leave
// them; slot 0's reset (0x893f0b70) and blocks of Vc2Signal's input (every eighth case a fade)
// through slot 1 (0x893f0c50), each block's flag drawn as the analysis's check draws it, and now and then before a
// block slot 5's settings (0x893f13e0, a drawn struct), the pedal through slot 6 (0x893f1960) or slot 4's warp
// (0x893f15d0, a side and a ratio).  After every slot the object's words (+0x004…+0x2ba4, the rate filters' states and
// the words their tables point at, +0x2cb4…+0x2d70), the analysis's state and slot 7's level (0x893f1a00), and after slot
// 5 the settings struct it writes back, word for word.  XT_VC2HIRESO_FIRST=1 says where the first case to differ does,
// XT_VC2HIRESO_STOP=1 stops there.  XT_VC2HIRESO_MAIN only.
#ifdef XT_VC2HIRESO_MAIN
#include "Vc2Hireso.h"
#include "Vc2HiresoCheck.h"
#include "Vc2Unvoiced.h"
#include "XtSwitches.h"
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

namespace {
constexpr uint32_t kConstruct = 0x893f0a10, kReset = 0x893f0b70, kAnalyze = 0x893f0c50, kWarp = 0x893f15d0;  // [FW]
constexpr uint32_t kSettings = 0x893f13e0, kHold = 0x893f1960, kLevel = 0x893f1a00;                          // [FW]
constexpr uint32_t kObjectBytes = 0x2d70, kRatesAt = 0x2ba4, kRateBytes = 68, kSettingWordsAt = 0x2cb4;    // [FW]
constexpr uint32_t kObjAt = 0x8C040000, kBlockAt = 0x8C035000;   // in Vc2Fw's RAM
constexpr int kBlocks = 400, kFadeEvery = 8, kEventOneIn = 8;

struct Counts {
    long settings = 0, holds = 0, warps = 0, unvoiced = 0, different = 0;
};

// The object's words, its rate filters' states and tables, its analysis's state and slot 7's level against rom.4's
std::string hiresoDifference(Vc2Fw& fw, const Vc2Hireso& e) {
    std::string what = vc2FirstDifference(fw, kObjAt + 4, &e.s, sizeof e.s, "the object");
    for (int k = 0; k < Vc2Hireso::kRates && what.empty(); ++k) {
        const uint32_t at = kObjAt + kRatesAt + kRateBytes * (uint32_t)k;
        const Vc2RateFilter& f = e.rates[k];
        what = vc2FirstDifference(fw, at + 4, f.s, sizeof f.s, "a rate filter's states");
        if (what.empty()) {
            const size_t bytes = sizeof(float) * Vc2HiresoTables::kDecimatorWords;
            what = vc2FirstDifference(fw, fw.cpu.rd32(at), f.c, bytes, "the words a rate filter's table points at");
        }
    }
    if (what.empty()) what = vc2FirstDifference(fw, kObjAt + kSettingWordsAt, &e.t, sizeof e.t, "the settings' words");
    if (what.empty()) what = vc2HiresoAnalysisDifference(fw, e.analysis);
    if (what.empty() && !(fw.call(kLevel, { kObjAt }) && vc2SameAsFr0(fw, e.holdLevel()))) what = "slot 7's level";
    return what;
}

// An index into Env Attack's or Release's curve: in range mostly, now and then past it, where slot 5 leaves it
int drawCurveIndex(std::mt19937& rng) {
    return rng() % 8 == 0 ? Vc2HiresoTables::kCurve + (int)(rng() % 27) : (int)(rng() % Vc2HiresoTables::kCurve);
}

// A settings struct: a template mostly a record pair's, now and then below 0 or a RAM curve's; the pivots mostly the
// template's (−1); the vocoder's words noise, which slot 5 doesn't read
Vc2HiresoSettings drawSettings(std::mt19937& rng) {
    Vc2HiresoSettings in {};
    for (uint32_t& w : in.vocoderLow) w = (uint32_t)rng();
    for (uint32_t& w : in.vocoderHigh) w = (uint32_t)rng();
    for (int32_t& p : in.pivot) p = rng() % 4 == 0 ? (int32_t)(rng() % 31) : -1;
    in.attack = drawCurveIndex(rng);
    in.release = drawCurveIndex(rng);
    constexpr int kPicks[8] = { -1, 16, 0, 0, 0, 0, 0, 0 };   // 0: any record pair; 16: a RAM curve, 16…20
    const int pick = kPicks[rng() % 8];
    in.formantTemplate = pick < 0 ? pick : pick + (int)(rng() % (pick ? 5 : 16));
    in.depth = (float)(rng() % 128) / 127.0f;
    in.dump = vc2DrawDump(rng);
    return in;
}

std::string settingsOn(Vc2Fw& fw, Vc2Hireso& e, std::mt19937& rng, Counts& n) {
    ++n.settings;
    Vc2HiresoSettings in = drawSettings(rng);
    fw.cpu.write(kVc2Args, &in, sizeof in);
    if (!fw.call(kSettings, { kObjAt, kVc2Args })) return "the settings fault: " + fw.cpu.error();
    e.settings(in);
    const std::string what = hiresoDifference(fw, e);
    return what.empty() ? vc2FirstDifference(fw, kVc2Args, &in, sizeof in, "the settings struct") : what;
}

std::string holdOn(Vc2Fw& fw, Vc2Hireso& e, std::mt19937& rng, Counts& n) {
    ++n.holds;
    const int controller = vc2DrawController(rng);
    if (!fw.call(kHold, { kObjAt, (uint32_t)controller })) return "the hold faults: " + fw.cpu.error();
    e.hold(controller);
    return hiresoDifference(fw, e);
}

std::string warpOn(Vc2Fw& fw, Vc2Hireso& e, std::mt19937& rng, Counts& n) {
    ++n.warps;
    const uint32_t side = rng() % 2;
    const float ratio = vc2DrawRatio(rng);
    fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(ratio);
    if (!fw.call(kWarp, { kObjAt, side })) return "slot 4 faults: " + fw.cpu.error();
    e.formant((int)side, ratio);
    return hiresoDifference(fw, e);
}

using Event = std::string (*)(Vc2Fw&, Vc2Hireso&, std::mt19937&, Counts&);
constexpr Event kEvents[] = { settingsOn, holdOn, warpOn };

// A block through both objects' slot 1, then the words that differ
std::string analyzeOn(Vc2Fw& fw, Vc2Hireso& e, const Vc2Unvoiced& stage, const float* x) {
    fw.cpu.write(kBlockAt, x, sizeof(float) * Vc2HiresoAnalysis::kBlock);
    if (!fw.call(kAnalyze, { kObjAt, kBlockAt })) return "slot 1 faults: " + fw.cpu.error();
    e.analyze(x, stage.voiced());
    return hiresoDifference(fw, e);
}

// The words only the synthesis reads or writes, noise on both, as a synthesis would leave them for the reset to clear
// or keep: the port's first, then all its words into rom.4's object
void scramble(Vc2Fw& fw, Vc2Hireso& e, std::mt19937& rng) {
    const std::pair<float*, size_t> spans[] = {
        { e.s.synthesis, std::size(e.s.synthesis) }, { &e.s.bank[0][0], sizeof e.s.bank / sizeof(float) },
        { &e.s.bandWords[0][0], sizeof e.s.bandWords / sizeof(float) }, { e.s.weight, std::size(e.s.weight) },
        { e.s.formant, std::size(e.s.formant) }, { &e.t.synthesis, 1 },
        { e.t.hold.synthesis, std::size(e.t.hold.synthesis) },
    };
    for (const auto& [words, n] : spans)
        for (size_t i = 0; i < n; ++i) words[i] = 2.0f * (float)rng() / 4294967296.0f - 1.0f;
    fw.cpu.write(kObjAt + 4, &e.s, sizeof e.s);
    fw.cpu.write(kObjAt + kSettingWordsAt, &e.t, sizeof e.t);
}

// Both constructed, rom.4's over zeros; what differs then
std::string constructOn(Vc2Fw& fw, std::unique_ptr<Vc2Hireso>& e) {
    const std::vector<uint8_t> zeros(kObjectBytes);
    fw.cpu.write(kObjAt, zeros.data(), zeros.size());
    vc2HiresoAnalysisZero(fw);
    if (!fw.call(kConstruct, { kObjAt })) return "the constructor faults: " + fw.cpu.error();
    e = std::make_unique<Vc2Hireso>();
    return hiresoDifference(fw, *e);
}
} // namespace

long vc2HiresoObjectCases(Vc2Fw& fw, const Vc2Run& run) {
    Vc2Cases cases { kReset, kFadeEvery, kBlocks, std::mt19937((uint32_t)run.seed), kObjAt };
    std::mt19937& rng = cases.rng;
    const bool first = xtSwitch("XT_VC2HIRESO_FIRST") != nullptr;
    Counts n;
    std::unique_ptr<Vc2Hireso> e;
    if (const std::string what = constructOn(fw, e); !what.empty()) {
        std::printf("hireso object: the constructor: %s\n", what.c_str());
        return 1;
    }
    Vc2Unvoiced stage;
    bool unvoiced = false;
    for (int c = 0; c < run.cases; ++c) {
        if (c > 0) scramble(fw, *e, rng);
        const std::string what = vc2AnalysisCase(fw, *e, hiresoDifference, cases, c, [&](const float* x) {
            const bool event = rng() % kEventOneIn == 0;
            const std::string said = event ? kEvents[rng() % std::size(kEvents)](fw, *e, rng, n) : "";
            unvoiced = vc2HiresoFlag(fw, stage, rng, unvoiced);
            n.unvoiced += unvoiced ? 1 : 0;
            return said.empty() ? analyzeOn(fw, *e, stage, x) : said;
        });
        if (what.empty()) continue;
        if (!n.different++ && first) std::printf("  object case %d, block %ld: %s\n", c, cases.blocksRun, what.c_str());
        if (xtSwitch("XT_VC2HIRESO_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("hireso object: %d cases, %ld blocks (%ld unvoiced), %ld settings, %ld pedals, %ld warps, "
                "%ld different\n", run.cases, cases.blocksRun, n.unvoiced, n.settings, n.holds, n.warps, n.different);
    return n.different;
}
#endif
