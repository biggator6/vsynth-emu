// xt_vc2vocoder <rom.4> [cases] [seed] (v220): the vocoder object, Vc2Vocoder, against rom.4's own static object
// (0x89999758) in Sh4Interp, after its file's static initializer (0x893f09b0, A440's period into 0x899d9d90) and its
// builder (0x893f0490).  Each case resets the input stage (0x893ecf60) on both, then blocks of Vc2Signal's mic and
// carriers through the block driver (mono 0x893f0510 or, the odd cases, stereo 0x893f0580): the mic into both mic rings
// where 0x893ed410 says, the glue (0x893f07a0) before each with a drawn scale and tracker's period (into 0x899d9bd0,
// 0x893ee010's word), and now and then before a block a mode switch (0x893f0600, any of 0…3), the settings path
// (0x893f06a0, a drawn struct), the pedal (0x893f0770) or the edit page's words (0x893f0960, 0x893f0970).  After every
// event and block the frames, the vocoder's words, the engine's and the hook's words, the edit words, both engines'
// objects and analyses and the stages' words (Vc2CheckWords) word for word, the stages' buffers at a case's end.
// XT_VC2VOCODER_FIRST=1 says where the first case to differ does, XT_VC2VOCODER_STOP=1 stops there.
// XT_VC2VOCODER_MAIN only.
#ifdef XT_VC2VOCODER_MAIN
#include "Vc2CheckVocoder.h"
#include "Vc2CheckWords.h"
#include "Vc2Vocoder.h"
#include "XtSwitches.h"
#include <bit>
#include <cstdio>
#include <memory>

namespace {
constexpr uint32_t kVocoder = 0x89999758, kStatic = 0x893f09b0, kBuild = 0x893f0490;                       // [FW]
constexpr uint32_t kMode = 0x893f0600, kSettings = 0x893f06a0, kHold = 0x893f0770, kGlue = 0x893f07a0;    // [FW]
constexpr uint32_t kMono = 0x893f0510, kStereo = 0x893f0580, kEditing = 0x893f0960, kEditSide = 0x893f0970;
constexpr uint32_t kMicAt = 0x893ed410, kStageReset = 0x893ecf60;                                         // [FW]
constexpr uint32_t kHookWord = 0x899d9d8c, kHiresoHook = 0x893ed420, kLegacyHook = 0x893f0480;            // [FW]
constexpr uint32_t kEditAt = 0x8994025c, kPeriodAt = 0x899d9bd0, kLevelModAt = 0x89997298;                // [FW]
constexpr uint32_t kHiresoAt = kVocoder + 0x24, kLegacyAt = kVocoder + 0x2d94, kEngineWord = kVocoder + 0x20;
constexpr uint32_t kFramesAt = 0x8C036000;   // in Vc2Fw's RAM
constexpr int kBlocks = 400, kEventOneIn = 10, kFrameWords = 2 * Vc2Unvoiced::kBlock;

struct Counts {
    long modes = 0, settings = 0, pedals = 0, edits = 0, playing = 0, different = 0;
};

// v345: the vocoder's compare is the checks' shared one (Vc2CheckVocoder.h), xt_vc2synth's too
std::string vocoderDifference(Vc2Fw& fw, const Vc2Vocoder& v) { return vc2VocoderDifference(fw, v); }

// The input stage's reset on the port, for vc2AnalysisCase's start (rom.4's is kStageReset)
struct StageStart {
    Vc2Vocoder& v;
    void reset() { v.stage.reset(); }
};
std::string startDifference(Vc2Fw& fw, const StageStart& s) { return vocoderDifference(fw, s.v); }

int drawVocoderMode(std::mt19937& rng) { return rng() % 8 == 0 ? 3 : (int)(rng() % 3); }   // now and then no mode

// The vocoder's whole settings struct, every word as the converter forms it: ratios mostly the ends and 1, offsets
// mostly 0
Vc2VocoderSettings drawVocoderSettings(std::mt19937& rng) {
    Vc2VocoderSettings in {};
    for (int side = 0; side < 2; ++side) {
        in.ratio[side] = vc2DrawRatio(rng);
        in.offset[side] = rng() % 3 == 0 ? (float)((int)(rng() % 9) - 4) / 8.0f : 0.0f;
        in.pivot[side] = rng() % 4 == 0 ? (int32_t)(rng() % 31) : -1;
    }
    in.soundLevel = vc2DrawLevel(rng);
    in.unvoiceLevel = vc2DrawLevel(rng);
    in.spread = (int32_t)(rng() % 2);
    in.detect = 24 + (int32_t)(rng() % 128);
    in.attack = (int32_t)(rng() % 101);
    in.release = (int32_t)(rng() % 101);
    in.formantTemplate = (int32_t)(rng() % 18);
    in.depth = vc2DrawLevel(rng);
    in.dump = vc2DrawDump(rng);
    return in;
}

// The vocoder's events of one int, rom.4's routine on (the vocoder, the value) and the port's method on it
struct IntEvent {
    uint32_t routine;
    const char* name;
    int (*draw)(std::mt19937&);
    void (Vc2Vocoder::*port)(int);
    long Counts::*count;
};
constexpr IntEvent kModeEvents { kMode, "the mode switch", drawVocoderMode, &Vc2Vocoder::setMode, &Counts::modes };
constexpr IntEvent kPedalEvents { kHold, "the pedal", vc2DrawController, &Vc2Vocoder::hold, &Counts::pedals };

std::string intEventOn(Vc2Fw& fw, Vc2Vocoder& v, std::mt19937& rng, Counts& n, const IntEvent& e) {
    ++(n.*e.count);
    const int value = e.draw(rng);
    return vc2BothOn(fw, e.routine, { kVocoder, (uint32_t)value }, e.name, [&] { (v.*e.port)(value); },
                     [&] { return vocoderDifference(fw, v); });
}

std::string modeOn(Vc2Fw& fw, Vc2Vocoder& v, std::mt19937& rng, Counts& n) {
    return intEventOn(fw, v, rng, n, kModeEvents);
}

std::string pedalOn(Vc2Fw& fw, Vc2Vocoder& v, std::mt19937& rng, Counts& n) {
    return intEventOn(fw, v, rng, n, kPedalEvents);
}

std::string vocoderSettingsOn(Vc2Fw& fw, Vc2Vocoder& v, std::mt19937& rng, Counts& n) {
    ++n.settings;
    Vc2VocoderSettings in = drawVocoderSettings(rng);
    fw.cpu.write(kVc2Args, &in, sizeof in);
    const std::string what = vc2BothOn(fw, kSettings, { kVocoder, kVc2Args }, "the settings path",
                                       [&] { v.settings(in); }, [&] { return vocoderDifference(fw, v); });
    return what.empty() ? vc2FirstDifference(fw, kVc2Args, &in, sizeof in, "the settings struct") : what;
}

std::string editOn(Vc2Fw& fw, Vc2Vocoder& v, std::mt19937& rng, Counts& n) {
    ++n.edits;
    const auto differs = [&] { return vocoderDifference(fw, v); };
    if (rng() % 3 == 0) {
        const int on = (int)(rng() % 2);
        return vc2BothOn(fw, kEditing, { (uint32_t)on }, "the edit flag", [&] { v.editing(on); }, differs);
    }
    const int pivot = (int)(rng() % 25), side = (int)(rng() % 3);
    const float ratio = vc2DrawRatio(rng), offset = (float)((int)(rng() % 9) - 4) / 8.0f;
    fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(ratio);
    fw.cpu.fb[0][5] = std::bit_cast<uint32_t>(offset);
    return vc2BothOn(fw, kEditSide, { (uint32_t)pivot, (uint32_t)side }, "the edit words",
                     [&] { v.editSide(pivot, side, ratio, offset); }, differs);
}

using Event = std::string (*)(Vc2Fw&, Vc2Vocoder&, std::mt19937&, Counts&);
constexpr Event kEvents[] = { modeOn, vocoderSettingsOn, vocoderSettingsOn, pedalOn, editOn };

// The glue on both with a scale and the tracker's period, then one block: the mic into both rings, the frames
// through both drivers; then the words that differ and the frames
std::string vocoderBlockOn(Vc2Fw& fw, Vc2Vocoder& v, const float* mic, float* frames, int channels,
                           std::mt19937& rng) {
    const float scale = rng() % 4 == 0 ? (float)(rng() % 128) / 127.0f : 1.0f;
    const float period = 20.0f + 400.0f * (float)rng() / 4294967296.0f;
    fw.cpu.wr32(kPeriodAt, std::bit_cast<uint32_t>(period));
    fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(scale);
    if (!fw.call(kGlue, { kVocoder })) return "the glue faults: " + fw.cpu.error();
    v.glue(scale, period);
    uint32_t at = 0;
    if (!fw.call(kMicAt, {}, &at)) return "the mic ring's write point faults";
    fw.cpu.write(at, mic, sizeof(float) * Vc2Unvoiced::kBlock);
    std::copy_n(mic, Vc2Unvoiced::kBlock, v.stage.micBlock());
    const float levelMod = rng() % 2 == 0 ? 0.0f : vc2DrawLevel(rng);
    fw.cpu.wr32(kLevelModAt, std::bit_cast<uint32_t>(levelMod));
    fw.cpu.write(kFramesAt, frames, sizeof(float) * kFrameWords);
    if (!fw.call(channels == 1 ? kMono : kStereo, { kVocoder, at, kFramesAt }))
        return "the block driver faults: " + fw.cpu.error();
    v.block(v.stage.micBlock(), frames, channels, levelMod);
    const std::string what = vc2FirstDifference(fw, kFramesAt, frames, sizeof(float) * kFrameWords, "the frames");
    return what.empty() ? vocoderDifference(fw, v) : what;
}

} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2vocoder",
                              vc2LoadAll<vc2LoadUnvoiced, vc2LoadLegacy, vc2LoadHireso, vc2LoadVocoder>);
    if (!run) return 1;
    Counts n;
    if (!fw.call(kStatic, {}) || !fw.call(kBuild, { kVocoder })) {
        std::printf("xt_vc2vocoder: the static initializer or the builder faults: %s\n", fw.cpu.error().c_str());
        return 1;
    }
    auto v = std::make_unique<Vc2Vocoder>();
    StageStart start { *v };
    Vc2Cases cases { kStageReset, 1 << 30, kBlocks, std::mt19937((uint32_t)run->seed) };   // no case fades
    std::mt19937& rng = cases.rng;
    const bool first = xtSwitch("XT_VC2VOCODER_FIRST") != nullptr;
    for (int c = 0; c < run->cases; ++c) {
        const int channels = 1 + c % 2;
        Vc2Signal left { std::mt19937(rng()), false }, right { std::mt19937(rng()), false };
        std::string what = vc2AnalysisCase(fw, start, startDifference, cases, c, [&](const float* x) {
            const bool event = rng() % kEventOneIn == 0;
            const std::string said = event ? kEvents[rng() % std::size(kEvents)](fw, *v, rng, n) : "";
            if (!said.empty()) return said;
            n.playing += v->engine ? 1 : 0;
            float frames[kFrameWords];
            vc2CarrierFrames(left, channels == 2 ? &right : nullptr, frames);
            return vocoderBlockOn(fw, *v, x, frames, channels, rng);
        });
        if (what.empty()) what = vc2UnvoicedBuffersDifference(fw, v->stage);
        if (what.empty()) continue;
        if (!n.different++ && first) std::printf("  case %d, block %ld: %s\n", c, cases.blocksRun, what.c_str());
        if (xtSwitch("XT_VC2VOCODER_STOP")) break;   // a plant's run: its first case that differs is enough
    }
    std::printf("vocoder: %d cases (%d stereo), %ld blocks (%ld playing), %ld modes, %ld settings, %ld pedals, "
                "%ld edits, %ld different\n", run->cases, run->cases / 2, cases.blocksRun, n.playing, n.modes,
                n.settings, n.pedals, n.edits, n.different);
    std::printf("xt_vc2vocoder: %ld different\n", n.different);
    return 0;
}
#endif
