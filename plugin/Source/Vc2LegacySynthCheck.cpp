// xt_vc2legacy's second part (v211, stereo v212): Vc2Legacy, LEGACY's object, against rom.4's on an object of its own
// in the interpreter's RAM, run twice, by its mono synthesis (slot 2, 0x893f25a0) and then by its stereo (slot 3,
// 0x893f2980).  Each run: its constructor (0x893f23c0) once, from the state the last cases leave; each case its reset
// (slot 0, 0x893f24b0), then blocks of the mic through slot 1 (0x893f2570) and of a carrier through the synthesis (mono:
// in each frame's first word, the second something else, to be seen left; stereo: a carrier of its own in each word),
// with the settings (slot 5, 0x893f2f90: a Formant Shift ratio and a dump time) and the pedal (slot 6, 0x893f2f80) now and
// then between them.  After every block and event the object's words (+0x004…+0x2ce4) and the analysis's state word for
// word; after a block besides the frames and slot 7's level (0x893f2fc0).  Generators of their own, so the analysis's
// cases stay as v210 drew them and mono's as v211 did.  XT_VC2LEGACY_MAIN only.
#ifdef XT_VC2LEGACY_MAIN
#include "Vc2LegacyCheck.h"
#include "XtSwitches.h"
#include <cstdio>

namespace {
constexpr uint32_t kConstruct = 0x893f23c0, kReset = 0x893f24b0, kAnalyze = 0x893f2570;   // [FW]
constexpr uint32_t kMono = 0x893f25a0, kStereo = 0x893f2980;                              // [FW]
constexpr uint32_t kSettings = 0x893f2f90, kHold = 0x893f2f80, kLevel = 0x893f2fc0;       // [FW]
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

// The settings or the pedal on both
std::string objectEventOn(Vc2Fw& fw, Vc2Legacy& engine, std::mt19937& rng, Counts& n) {
    if (rng() % 2 == 0) {
        ++n.settings;
        const float ratio = vc2DrawRatio(rng), dump = vc2DrawDump(rng);
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

// The carriers at the bus's scale: the left's in each frame's first word; the right's, when there is one, in the
// second, else something mono leaves
void carrierFrames(Vc2Signal& left, Vc2Signal* right, float* frames) {
    float x[kBlock], r[kBlock];
    left.block(x);
    if (right) right->block(r);
    for (int i = 0; i < kBlock; ++i) {
        frames[2 * i] = x[i] * 1073741824.0f;
        frames[2 * i + 1] = right ? r[i] * 1073741824.0f : -0.5f * x[i];
    }
}

// A block of the mic through slot 1 and of the carriers through the synthesis on both, then the first word that differs
std::string engineBlockOn(Vc2Fw& fw, Vc2Legacy& engine, const float* mic, float* frames, int channels) {
    fw.cpu.write(kVc2LegacyBlockAt, mic, sizeof(float) * kBlock);
    if (!fw.call(kAnalyze, { kObjAt, kVc2LegacyBlockAt })) return "slot 1 faults: " + fw.cpu.error();
    engine.analyze(mic);
    fw.cpu.write(kFramesAt, frames, sizeof(float) * kFrameWords);
    const uint32_t synthesis = channels == 1 ? kMono : kStereo;
    if (!fw.call(synthesis, { kObjAt, kFramesAt })) return "the synthesis faults: " + fw.cpu.error();
    channels == 1 ? engine.mono(frames) : engine.stereo(frames);
    std::string what = vc2FirstDifference(fw, kFramesAt, frames, sizeof(float) * kFrameWords, "the frames");
    if (what.empty()) what = objectDifference(fw, engine);
    const bool level = fw.call(kLevel, { kObjAt }) && vc2SameAsFr0(fw, engine.holdLevel());
    if (what.empty() && !level) what = "slot 7's level";
    return what;
}
} // namespace

long vc2LegacySynthesisCases(Vc2Fw& fw, Vc2LegacyAnalysis& carried, const Vc2Run& run, int channels) {
    std::mt19937 rng((uint32_t)run.seed * 2654435761u + (uint32_t)channels);
    Counts n;
    Vc2Legacy engine;
    engine.analysis = carried;   // rom.4's globals as the last cases left them, its .data words with them
    engine.analysis.reset();     // the constructor's init
    // memory of its own, zeroed as the first run's was: the constructor leaves the rows, which a block writes first
    const std::vector<uint8_t> fresh(sizeof(Vc2LegacyWords) + 4, 0);
    fw.cpu.write(kObjAt, fresh.data(), fresh.size());
    std::string made = fw.call(kConstruct, { kObjAt }) ? objectDifference(fw, engine) : "the constructor faults";
    for (int c = 0; c < run.cases; ++c) {
        const bool fades = c % kVc2LegacyFadeEvery == kVc2LegacyFadeEvery - 1;
        Vc2Signal mic { std::mt19937(rng()), fades }, carrier { std::mt19937(rng()) };
        Vc2Signal right { std::mt19937(channels == 2 ? rng() : 0u) };
        std::string what = c == 0 ? made : "";
        if (what.empty()) what = vc2ResetOn(fw, kReset, { kObjAt }, engine, objectDifference);
        float x[kBlock], frames[kFrameWords];
        for (int k = 0; k < (fades ? kVc2FadeBlocks : kVc2LegacyBlocks) && what.empty(); ++k, ++n.blocks) {
            if (rng() % 24 == 0) {
                ++n.events;
                what = objectEventOn(fw, engine, rng, n);
            }
            mic.block(x);
            carrierFrames(carrier, channels == 2 ? &right : nullptr, frames);
            if (what.empty()) what = engineBlockOn(fw, engine, x, frames, channels);
            n.held += engine.analysis.s.controller >= 64 ? 1 : 0;
        }
        if (what.empty()) continue;
        if (!n.different++ && vc2LegacyFirst())
            std::printf("  object case %d (%d channels), block %ld: %s\n", c, channels, n.blocks, what.c_str());
        if (xtSwitch("XT_VC2LEGACY_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("legacy %ssynthesis: %d cases, %ld blocks, %ld events (%ld settings, %ld blocks held), %ld different\n",
                channels == 2 ? "stereo " : "", run.cases, n.blocks, n.events, n.settings, n.held, n.different);
    carried = engine.analysis;
    return n.different;
}
#endif
