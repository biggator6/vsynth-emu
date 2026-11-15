// xt_vc2legacy's second part (v211): Vc2Legacy, LEGACY's object, against rom.4's on an object of its own in the
// interpreter's RAM.  Its constructor (0x893f23c0) once, from the state the analysis's cases leave; each case its reset
// (slot 0, 0x893f24b0), then blocks of the mic through slot 1 (0x893f2570) and of a carrier through the mono synthesis
// (slot 2, 0x893f25a0), with the settings (slot 5, 0x893f2f90: a Formant Shift ratio and a dump time) and the pedal (slot
// 6, 0x893f2f80) now and then between them.  After every block and event the object's words (+0x004…+0x2ce4) and the
// analysis's state word for word; after a block besides the frames and slot 7's level (0x893f2fc0).  A generator of its
// own, so the analysis's cases stay as v210 drew them.  XT_VC2LEGACY_MAIN only.
#ifdef XT_VC2LEGACY_MAIN
#include "Vc2LegacyCheck.h"
#include "XtSwitches.h"
#include <cstdio>

namespace {
constexpr uint32_t kConstruct = 0x893f23c0, kReset = 0x893f24b0, kAnalyze = 0x893f2570, kMono = 0x893f25a0;   // [FW]
constexpr uint32_t kSettings = 0x893f2f90, kHold = 0x893f2f80, kLevel = 0x893f2fc0;                         // [FW]
constexpr uint32_t kDumpAt = 0x38;   // [FW] the settings' Hold Dump Time, after the Formant Shift ratio at +0
constexpr uint32_t kObjAt = 0x8C020000, kFramesAt = 0x8C012000;   // in Vc2Fw's RAM
constexpr int kBlock = Vc2LegacyAnalysis::kBlock, kFrameWords = 2 * kBlock;

struct Counts {
    long blocks = 0, events = 0, settings = 0, held = 0, different = 0;
};

// The object's words and the analysis's state against rom.4's
std::string objectDifference(Vc2Fw& fw, const Vc2Legacy& engine) {
    std::string what = vc2FirstDifference(fw, kObjAt + 4, &engine.s, sizeof engine.s, "the object");
    if (what.empty()) what = vc2LegacyStateDifference(fw, engine.analysis);
    return what;
}

// A Formant Shift ratio, 0.5 to 2: its ends and 1 most often
float drawRatio(std::mt19937& rng) {
    constexpr float kRatios[6] = { 0.5f, 1.0f, 2.0f, -1.0f, -1.0f, -1.0f };   // -1: any
    const float pick = kRatios[rng() % 6];
    return pick < 0.0f ? 0.5f + 1.5f * (float)rng() / 4294967296.0f : pick;
}

// The settings or the pedal on both
std::string objectEventOn(Vc2Fw& fw, Vc2Legacy& engine, std::mt19937& rng, Counts& n) {
    if (rng() % 2 == 0) {
        ++n.settings;
        const float ratio = drawRatio(rng), dump = vc2DrawDump(rng);
        fw.cpu.wr32(kVc2Args, std::bit_cast<uint32_t>(ratio));
        fw.cpu.wr32(kVc2Args + kDumpAt, std::bit_cast<uint32_t>(dump));
        if (!fw.call(kSettings, { kObjAt, kVc2Args })) return "the settings fault: " + fw.cpu.error();
        engine.settings(ratio, dump);
        return objectDifference(fw, engine);
    }
    const int controller = vc2DrawController(rng);
    if (!fw.call(kHold, { kObjAt, (uint32_t)controller })) return "the hold faults: " + fw.cpu.error();
    engine.hold(controller);
    return objectDifference(fw, engine);
}

// The carrier's block in each frame's first word, at the bus's scale; in the second, which mono leaves, something else
void carrierFrames(Vc2Signal& carrier, float* frames) {
    float x[kBlock];
    carrier.block(x);
    for (int i = 0; i < kBlock; ++i) {
        frames[2 * i] = x[i] * 1073741824.0f;
        frames[2 * i + 1] = -0.5f * x[i];
    }
}

// A block of the mic through slot 1 and of the carrier through slot 2 on both, then the first word that differs
std::string engineBlockOn(Vc2Fw& fw, Vc2Legacy& engine, const float* mic, float* frames) {
    fw.cpu.write(kVc2LegacyBlockAt, mic, sizeof(float) * kBlock);
    if (!fw.call(kAnalyze, { kObjAt, kVc2LegacyBlockAt })) return "slot 1 faults: " + fw.cpu.error();
    engine.analyze(mic);
    fw.cpu.write(kFramesAt, frames, sizeof(float) * kFrameWords);
    if (!fw.call(kMono, { kObjAt, kFramesAt })) return "the mono synthesis faults: " + fw.cpu.error();
    engine.mono(frames);
    std::string what = vc2FirstDifference(fw, kFramesAt, frames, sizeof(float) * kFrameWords, "the frames");
    if (what.empty()) what = objectDifference(fw, engine);
    const bool level = fw.call(kLevel, { kObjAt }) && vc2SameAsFr0(fw, engine.holdLevel());
    if (what.empty() && !level) what = "slot 7's level";
    return what;
}
} // namespace

long vc2LegacySynthesisCases(Vc2Fw& fw, const Vc2LegacyAnalysis& carried, const Vc2Run& run) {
    std::mt19937 rng((uint32_t)run.seed * 2654435761u + 1u);
    Counts n;
    Vc2Legacy engine;
    engine.analysis = carried;   // rom.4's globals as the analysis's cases left them, its .data words with them
    engine.analysis.reset();     // the constructor's init
    std::string made = fw.call(kConstruct, { kObjAt }) ? objectDifference(fw, engine) : "the constructor faults";
    for (int c = 0; c < run.cases; ++c) {
        const bool fades = c % kVc2LegacyFadeEvery == kVc2LegacyFadeEvery - 1;
        Vc2Signal mic { std::mt19937(rng()), fades }, carrier { std::mt19937(rng()) };
        std::string what = c == 0 ? made : "";
        if (what.empty()) what = vc2ResetOn(fw, kReset, { kObjAt }, engine, objectDifference);
        float x[kBlock], frames[kFrameWords];
        for (int k = 0; k < (fades ? kVc2FadeBlocks : kVc2LegacyBlocks) && what.empty(); ++k, ++n.blocks) {
            if (rng() % 24 == 0) {
                ++n.events;
                what = objectEventOn(fw, engine, rng, n);
            }
            mic.block(x);
            carrierFrames(carrier, frames);
            if (what.empty()) what = engineBlockOn(fw, engine, x, frames);
            n.held += engine.analysis.s.controller >= 64 ? 1 : 0;
        }
        if (what.empty()) continue;
        if (!n.different++ && vc2LegacyFirst())
            std::printf("  object case %d, block %ld: %s\n", c, n.blocks, what.c_str());
        if (xtSwitch("XT_VC2LEGACY_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("legacy synthesis: %d cases, %ld blocks, %ld events (%ld settings, %ld blocks held), %ld different\n",
                run.cases, n.blocks, n.events, n.settings, n.held, n.different);
    return n.different;
}
#endif
