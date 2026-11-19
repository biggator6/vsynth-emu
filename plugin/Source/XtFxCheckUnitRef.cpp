// xt_fx's unit against the reference (v274): XtFxUnit's process(), the frame into the chip at the image's gain and the
// mix and DIR out, against the same driver on JE-8086's chip given each frame as the image's own truncation gives it, the
// whole state each sample, with edits from the image's own ranges between.  The outside input's slots are silent on
// both.  Where DIR goes nowhere (rom.4), DIR out must stay silent.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckNoise.h"
#include "XtFxCheckUnit.h"

namespace xtfxcheck {
namespace {
constexpr int kFrameWords = 8;         // the frame's words, the first of InputNoise's inputs
constexpr int kInputSlots = InputNoise::kCount - kFrameWords;

// The bus a block of 32 frames at a time: noise for the frame's words at the firmware's units, with a fraction under a
// chip word's step so the truncation's direction shows; then the image's truncation of it, and the same frames as the
// plugin's bus gives them to the unit, the firmware's over the check's own gain
struct FrameBlock {
    static constexpr int kFrames = Truncation::kWords / kFrameWords;
    const UnitWay& way;
    InputNoise noise;
    Rng fraction;
    Truncation fw;
    float firmware[Truncation::kWords] = {}, plugin[Truncation::kWords] = {};
    int32_t words[Truncation::kWords] = {};
    int at = kFrames;
    FrameBlock(const char* rom, const UnitWay& w, uint32_t seed)
        : way(w), noise(seed), fraction(seed + 1), fw(rom, w) {}
    // Moves to the next frame, from `sample` on; false when the image's truncation faults
    bool next(long sample) {
        if (++at < kFrames) return true;
        at = 0;
        for (int f = 0; f < kFrames; f++) {
            int32_t x[InputNoise::kCount];
            noise.next(sample + f, x);
            for (int w = 0; w < kFrameWords; w++) {
                const int i = kFrameWords * f + w;
                firmware[i] = (float)x[w] * 256.0f + (float)(fraction.below(511) - 255);
                plugin[i] = firmware[i] / way.frameGain;
            }
        }
        return fw.run(firmware, words);
    }
    const float* frame() const { return plugin + kFrameWords * at; }
    int32_t word(int w) const { return words[kFrameWords * at + w] >> 8; }
};
} // namespace

void unitAgainstReference(const char* rom, int randomEvents, uint32_t seed, const UnitWay& way) {
    auto ref = std::make_unique<Reference>();
    auto unit = std::make_unique<XtFxUnit>(rom, way.fx.image);
    auto block = std::make_unique<FrameBlock>(rom, way, seed);
    long samples = 0, different = 0, firstAt = -1, outs = 0, dir = 0, faults = 0;
    std::string what;
    const auto sample = [&] {
        faults += block->next(samples) ? 0 : 1;
        float lr[2] = {};
        unit->process(block->frame(), lr, 1);
        auto& rt = ref->runtime();
        for (int w = 0; w < kFrameWords; w++) rt.writeGram(block->word(w), (uint8_t)InputNoise::kInputs[w]);
        for (int i = 0; i < kInputSlots; i++) rt.writeGram(0, (uint8_t)InputNoise::kInputs[kFrameWords + i]);
        ref->runProgram(768);
        ref->sync();
        samples++;
        const std::string d = differs(unit->chip(), *ref, samples % 2048 == 0);
        if (!d.empty()) {
            if (!different) {
                firstAt = samples;
                what = d;
            }
            different++;
        }
        // one-off: the outputs as the check reads them, apart from XtFxUnit::process's: the mix, then DIR, a side each
        const float left = XtFxUnit::fromChip(rt.readGram(0xa1)) + XtFxUnit::fromChip(rt.readGram(0xa5));
        const float right = XtFxUnit::fromChip(rt.readGram(0xa3)) + XtFxUnit::fromChip(rt.readGram(0xa7));
        outs += left != lr[0] || right != lr[1];
        dir += !way.dirOut && (rt.readGram(0xa5) != 0 || rt.readGram(0xa7) != 0);
    };
    DirectBus bus(*ref);
    bus.sample = sample;
    XtFxControl direct(rom, &bus, way.fx.image);                    // starts the reference: its start's waits run samples
    Editor ed(rom, unit->control(), seed + 2, way.fx);
    for (int i = 0; i < randomEvents; i++) {
        const Edit e = ed.any();
        apply(unit->control(), e);
        apply(direct, e);
        const int after = ed.r.below(4) ? ed.r.below(64) : ed.r.below(2048);
        for (int s = 0; s < after; s++) sample();
    }
    if (different && xtSwitch("XT_FX_FIRST"))
        std::printf("  %s frame through the unit: first at sample %ld, %s\n", way.whose, firstAt, what.c_str());
    if ((outs || dir || faults) && xtSwitch("XT_FX_FIRST"))
        std::printf("  %s frame through the unit: %ld outputs apart, %ld with DIR out, %ld faults\n", way.whose, outs, dir,
                    faults);
    const bool ran = unit->ok() && direct.ok();
    if (!ran) std::printf("  %s frame through the unit: a fault: %s %s\n", way.whose, unit->control().error().c_str(),
                          direct.error().c_str());
    std::printf("%s frame through the unit against the reference: %ld samples, %ld different\n", way.whose, samples,
                different + outs + dir + faults + (ran ? 0 : 1));
}
} // namespace xtfxcheck
#endif
