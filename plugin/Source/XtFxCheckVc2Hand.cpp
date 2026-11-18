// xt_fx <rom.4> vc2's two parts by hand: rom.4's driver run with this file's own addresses, store and message bytes,
// against XtFxControl's, every word after each change.  That's the image's addresses, the hooks, the store's defaults, the
// message's flags and the poll of an outside clock.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckVc2.h"

namespace xtfxcheck {
namespace {
// [FW] rom.4's driver as this file knows it, written out apart from XtFxImage.cpp so that an address wrong there shows
constexpr XtFxImage kHandImage = {
    .self = 0x89980f24, .message = 0x89980f84, .clock = 0x8995cb40,
    .descLow = 0x8950f0b0, .descHigh = 0x895131a8, .firstHigh = 0x56a, .ids = 0x9ac,
    .mfxIds = 0x895164ec, .choIds = 0x895165b8, .revIds = 0x895165e4,
    .mfxTypes = 42, .choTypes = 9, .revTypes = 15,
    .staticInit = 0x89368280, .start = 0x8935f9a0, .startTask = 0x8935fa80,
    .takeMessage = 0x8935fc40, .clockPoll = 0x8935fde0,
    .matrixTake = 0, .matrixApply = 0, .inputRequest = 0, .inputEvent = 0,
    .sleep = 0x89365e50, .putWord = 0x89362490, .getWord = 0x89362610,
    .putCoefficient = 0x893626d0, .putOffset = 0x893627f0,
    .reads = { 0x89324350, 0x89324370 },
    .answered = { 0x89362430, 0x89362450, 0x893951b0, 0x89395040, 0x89394fa0, 0x89301610, 0x89391e50 },
    .idRows = {},
};

// rom.4's driver by hand: the bus as Rig has it, a store of this file's own and the message's flags written here
struct HandDriver : Rig {
    const XtFxImage& im;
    std::vector<int16_t> store;
    explicit HandDriver(const char* rom) : Rig(rom, false, kHandImage), im(kHandImage), store((size_t)im.ids) {
        c.mapRam(0, 0x1000);
        for (const uint32_t fn : im.answered)
            if (fn) c.hook(fn, [](Sh4& m) { m.ret(0); });
        for (int id = 0; id < im.ids; id++) store[(size_t)id] = (int16_t)defaultOf(id);
        for (const uint32_t fn : im.reads)
            c.hook(fn, [this](Sh4& m) {
                const int id = (int16_t)m.r[5];
                m.ret((uint32_t)(int32_t)(id >= 0 && id < im.ids ? store[(size_t)id] : 0));
            });
        call(im.staticInit);
        call(im.start, { im.self });
        call(im.startTask, { im.self });
        message(kSystem, kPatch);
    }
    int defaultOf(int id) {
        if (id >= im.firstHigh) return s16(im.descHigh + 8 * (uint32_t)(id - im.firstHigh) + 2);
        const uint32_t d = im.descLow + 12 * (uint32_t)id;
        return (uint16_t)c.rd16(d + 6) - (uint16_t)c.rd16(d + 4);
    }
    void message(uint32_t second, uint32_t fourth) {
        c.wr8(im.message + 1, (uint8_t)second);
        c.wr8(im.message + 3, (uint8_t)fourth);
        call(im.takeMessage, { im.self });
    }
    // The poll of an outside clock: the clock's object holds its source at +0x40 and the ms over six clocks at +0x48
    void poll(int sixClocksMs) {
        c.wr8(im.clock + 0x40, 1);
        c.wr16(im.clock + 0x48, (uint16_t)sixClocksMs);
        call(im.clockPoll, { im.self });
    }
};
} // namespace

void handMessages(const char* rom, int events, uint32_t seed) {
    auto hand = std::make_unique<HandDriver>(rom);
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
    auto hand = std::make_unique<HandDriver>(rom);
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
