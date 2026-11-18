// xt_fx <rom.4> vc2's two parts by hand: rom.4's driver run with this file's own addresses, store and message bytes,
// against XtFxControl's, every word after each change.  That's the image's addresses, the hooks, the store's defaults, the
// message's flags and the poll of an outside clock.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckVc2.h"

namespace xtfxcheck {
void handMessages(const char* rom, int events, uint32_t seed) {
    auto hand = std::make_unique<HandDriver>(rom, handRom4());
    XtFxControl c(rom, nullptr, vc2Fx());
    Editor ed(rom, c, seed + 500, vc2Ids());
    int tried = 1, different = wordsApart(c, hand->chip) ? 1 : 0;
    std::string first = different ? "the start" : "";
    for (int i = 0; i < events * 8; i++) {
        const Edit e = ed.any();
        if (e.matrix || e.patch) continue;
        uint32_t second = 0, fourth = 0;
        for (const auto& [id, value] : e.sets) {
            const int v = std::clamp(value, c.lowest(id), c.highest(id));
            if (hand->store[(size_t)id] == v) continue;
            hand->store[(size_t)id] = (int16_t)v;
            const IdBlock* b = listedBlock(id);
            second |= b ? b->second : 0;
            fourth |= b ? b->fourth : 0;
        }
        apply(c, e);
        if (second | fourth) hand->message(second, fourth);
        tried++;
        if (c.ok() && wordsApart(c, hand->chip) == 0) continue;
        if (first.empty()) first = "edit " + std::to_string(i) + (c.ok() ? "" : ": " + c.error());
        different++;
    }
    // The patch's Category, which the switches' reader reads and no live edit sends: every value leaves the words alone,
    // and a patch of each loads, 10 and 11 posting an event to another task
    const int category = c.get(kCategory);
    for (int v = c.lowest(kCategory); v <= c.highest(kCategory) + 1; v++) {
        const int value = v > c.highest(kCategory) ? category : v;
        hand->store[(size_t)kCategory] = (int16_t)value;
        hand->message(0, kSwitches);
        const int moved = wordsApart(c, hand->chip);
        editOne(c, kCategory, value, true);
        hand->message(0, kPatch);
        tried++;
        if (moved == 0 && c.ok() && wordsApart(c, hand->chip) == 0) continue;
        if (first.empty()) first = "the Category at " + std::to_string(value);
        different++;
    }
    if (different && xtSwitch("XT_FX_FIRST")) std::printf("  messages: %s\n", first.c_str());
    std::printf("vc2 control's messages against the driver by hand: %d samples, %d different\n", tried, different);
}

// The outside clock from a new patch, on an MFX type that follows the tempo: readings inside the poll's margin and past
// it.  Some reading must move the chip's words, or this shows nothing.
void handOutsideClock(const char* rom) {
    constexpr int kSyncedMfx = 11, kMfxSwitch = 0x13d;
    auto hand = std::make_unique<HandDriver>(rom, handRom4());
    XtFxControl c(rom, nullptr, vc2Fx());
    const FxIds& fx = vc2Ids();
    for (const auto& [id, value] : { std::pair { kMfxSwitch, 1 }, std::pair { fx.mfxType, kSyncedMfx } }) {
        c.set(id, value);
        hand->store[(size_t)id] = (int16_t)value;
    }
    c.loadPatch();
    hand->message(0, kPatch);
    editOne(c, fx.clockSource, 1);
    hand->store[(size_t)fx.clockSource] = 1;
    hand->message(kSystem, 0);
    int tried = 0, different = 0, moves = 0;
    for (const int ms : { 125, 125, 122, 128, 129, 125, 90 }) {
        const auto before = std::make_unique<XtFxChip>(hand->chip);
        c.outsideClock(ms);
        hand->poll(ms);
        moves += wordsApart(*before, hand->chip) != 0;
        tried++;
        different += c.ok() && wordsApart(c, hand->chip) == 0 ? 0 : 1;
    }
    tried++;
    different += moves ? 0 : 1;
    if (!moves && xtSwitch("XT_FX_FIRST")) std::printf("  the outside clock: no reading moved a word\n");
    std::printf("vc2 control's outside clock against the driver by hand: %d samples, %d different\n", tried, different);
}
} // namespace xtfxcheck
#endif
